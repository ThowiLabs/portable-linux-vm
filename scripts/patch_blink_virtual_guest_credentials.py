#!/usr/bin/env python3
"""Reemplaza compatibilidad provisional UID0 de Blink con credenciales guest aisladas.
Compatibilidad específica para Windows: nunca llamar a setuid/setgid del host.
"""
from pathlib import Path
p=Path('build/blink-src/blink/syscall.c')
s=p.read_text()
start=s.index('static bool PortableBoxIsRootOrUnchanged(u32 id) {')
end=s.index('static int SysSchedYield(struct Machine *m)',start)
original=s[start:end]
Path('/tmp/portablebox-identity-old-block.c').write_text(original)
replacement=r'''static void PortableBoxGuestSetuids(struct System *s, u32 real, u32 effective, u32 saved) {
  if (real != (u32)-1) s->guest_ruid = real;
  if (effective != (u32)-1) s->guest_euid = effective;
  if (saved != (u32)-1) s->guest_suid = saved;
}

static void PortableBoxGuestSetgids(struct System *s, u32 real, u32 effective, u32 saved) {
  if (real != (u32)-1) s->guest_rgid = real;
  if (effective != (u32)-1) s->guest_egid = effective;
  if (saved != (u32)-1) s->guest_sgid = saved;
}

static int PortableBoxGuestChangeUid(struct System *s, u32 real, u32 effective,
                                     u32 saved) {
  // Linux: root may change any UID; unprivileged processes may select
  // only their real/saved UID for the effective UID.
  if (s->guest_euid != 0) {
    if (real != (u32)-1 && real != s->guest_ruid && real != s->guest_euid)
      return eperm();
    if (effective != (u32)-1 && effective != s->guest_ruid &&
        effective != s->guest_euid && effective != s->guest_suid)
      return eperm();
    if (saved != (u32)-1 && saved != s->guest_suid)
      return eperm();
  }
  PortableBoxGuestSetuids(s, real, effective, saved);
  return 0;
}

static int PortableBoxGuestChangeGid(struct System *s, u32 real, u32 effective,
                                     u32 saved) {
  if (s->guest_euid != 0) {
    if (real != (u32)-1 && real != s->guest_rgid && real != s->guest_egid)
      return eperm();
    if (effective != (u32)-1 && effective != s->guest_rgid &&
        effective != s->guest_egid && effective != s->guest_sgid)
      return eperm();
    if (saved != (u32)-1 && saved != s->guest_sgid)
      return eperm();
  }
  PortableBoxGuestSetgids(s, real, effective, saved);
  return 0;
}

static int SysGetuid(struct Machine *m) {
  return PortableBoxGuestRoot() ? m->system->guest_ruid : getuid();
}

static int SysGetgid(struct Machine *m) {
  return PortableBoxGuestRoot() ? m->system->guest_rgid : getgid();
}

static int SysGeteuid(struct Machine *m) {
  return PortableBoxGuestRoot() ? m->system->guest_euid : geteuid();
}

static int SysGetegid(struct Machine *m) {
  return PortableBoxGuestRoot() ? m->system->guest_egid : getegid();
}

static i32 SysGetgroups(struct Machine *m, i32 size, i64 addr) {
  gid_t *group;
  u8 i32buf[4];
  int i, ngroups;
  long ngroups_max;
  if (PortableBoxGuestRoot()) {
    const int count = m->system->guest_ngroups;
    if (size < 0) return einval();
    if (!size) return count;
    if (size < count) return einval();
    if (count && !IsValidMemory(m, addr, (size_t)count * 4, PROT_WRITE))
      return efault();
    for (i = 0; i < count; ++i) {
      Write32(i32buf, m->system->guest_groups[i]);
      if (CopyToUserWrite(m, addr + (size_t)i * 4, i32buf, 4) == -1)
        return -1;
    }
    return count;
  }
  if (!size) return getgroups(0, 0);
#ifdef __APPLE__
  ngroups_max = size;
#else
  ngroups_max = sysconf(_SC_NGROUPS_MAX);
#endif
  size = MIN(size, ngroups_max);
  if (!IsValidMemory(m, addr, (size_t)size * 4, PROT_WRITE)) return -1;
  if (!(group = (gid_t *)AddToFreeList(m, malloc(size * sizeof(gid_t)))))
    return -1;
  if ((ngroups = getgroups(size, group)) != -1) {
    for (i = 0; i < ngroups; ++i) {
      Write32(i32buf, group[i]);
      CopyToUserWrite(m, addr + (size_t)i * 4, i32buf, 4);
    }
  }
  return ngroups;
}

static i32 SysSetgroups(struct Machine *m, i32 size, i64 addr) {
#ifdef HAVE_SETGROUPS
  int i;
  gid_t *group;
  const u8 *group_linux;
  if (PortableBoxGuestRoot()) {
    if (m->system->guest_euid != 0) return eperm();
    if (size < 0 || size > 32) return einval();
    if (size) {
      if (!(group_linux = (const u8 *)SchlepR(m, addr, (size_t)size * 4)))
        return -1;
      for (i = 0; i < size; ++i)
        m->system->guest_groups[i] = Read32(group_linux + (size_t)i * 4);
    }
    m->system->guest_ngroups = size;
    return 0;
  }
  if (!(group_linux = (const u8 *)SchlepR(m, addr, (size_t)size * 4)) ||
      !(group = (gid_t *)AddToFreeList(m, malloc(size * sizeof(gid_t)))))
    return -1;
  for (i = 0; i < size; ++i) group[i] = Read32(group_linux + (size_t)i * 4);
  return setgroups(size, group);
#else
  return enosys();
#endif
}

static i32 SysSetresuid(struct Machine *m, u32 real, u32 effective, u32 saved) {
  if (PortableBoxGuestRoot())
    return PortableBoxGuestChangeUid(m->system, real, effective, saved);
#ifdef HAVE_SETRESUID
  return setresuid(real, effective, saved);
#elif defined(HAVE_SETREUID)
  return setreuid(real, effective);
#else
  if (real == -1 && effective == -1) return 0;
  if (real == -1) return seteuid(effective);
  if (real != effective) return enosys();
  return setuid(real);
#endif
}

static i32 SysSetresgid(struct Machine *m, u32 real, u32 effective, u32 saved) {
  if (PortableBoxGuestRoot())
    return PortableBoxGuestChangeGid(m->system, real, effective, saved);
#ifdef HAVE_SETRESGID
  return setresgid(real, effective, saved);
#elif defined(HAVE_SETREGID)
  return setregid(real, effective);
#else
  if (real == -1 && effective == -1) return 0;
  if (real == -1) return setegid(effective);
  if (real != effective) return enosys();
  return setgid(real);
#endif
}

static int SysSetreuid(struct Machine *m, u32 real, u32 effective) {
  if (PortableBoxGuestRoot()) {
    const u32 old_real = m->system->guest_ruid;
    const int result = PortableBoxGuestChangeUid(m->system, real, effective, (u32)-1);
    if (result == 0 && (real != (u32)-1 || (effective != (u32)-1 && effective != old_real)))
      m->system->guest_suid = m->system->guest_euid;
    return result;
  }
#ifdef HAVE_SETRESUID
  if (real != -1 || (effective != -1 && effective != getuid())) {
    if (effective == -1) effective = geteuid();
    return setresuid(real, effective, effective);
  } else {
    return setresuid(real, effective, -1);
  }
#else
  return SysSetresuid(m, real, effective, -1);
#endif
}

static int SysSetregid(struct Machine *m, u32 real, u32 effective) {
  if (PortableBoxGuestRoot()) {
    const u32 old_real = m->system->guest_rgid;
    const int result = PortableBoxGuestChangeGid(m->system, real, effective, (u32)-1);
    if (result == 0 && (real != (u32)-1 || (effective != (u32)-1 && effective != old_real)))
      m->system->guest_sgid = m->system->guest_egid;
    return result;
  }
#ifdef HAVE_SETRESUID
  if (real != -1 || (effective != -1 && effective != getgid())) {
    if (effective == -1) effective = getegid();
    return setresgid(real, effective, effective);
  } else {
    return setresgid(real, effective, -1);
  }
#else
  return SysSetresgid(m, real, effective, -1);
#endif
}

static i32 SysGetresuid(struct Machine *m, i64 realaddr, i64 effectiveaddr,
                        i64 savedaddr) {
  u8 *real = 0;
  u8 *saved = 0;
  u8 *effective = 0;
  uid_t uid, euid, suid;
  if ((realaddr && !(real = (u8 *)SchlepW(m, realaddr, 4))) ||
      (savedaddr && !(saved = (u8 *)SchlepW(m, savedaddr, 4))) ||
      (effectiveaddr && !(effective = (u8 *)SchlepW(m, effectiveaddr, 4))))
    return -1;
  if (PortableBoxGuestRoot()) {
    uid = m->system->guest_ruid;
    euid = m->system->guest_euid;
    suid = m->system->guest_suid;
  } else {
#ifdef HAVE_SETRESUID
    if (getresuid(&uid, &euid, &suid) == -1) return -1;
#else
    uid = getuid();
    euid = geteuid();
    suid = euid;
#endif
  }
  if (real) Write32(real, uid);
  if (saved) Write32(saved, suid);
  if (effective) Write32(effective, euid);
  return 0;
}

static i32 SysGetresgid(struct Machine *m, i64 realaddr, i64 effectiveaddr,
                        i64 savedaddr) {
  u8 *real = 0;
  u8 *saved = 0;
  u8 *effective = 0;
  gid_t gid, egid, sgid;
  if ((realaddr && !(real = (u8 *)SchlepW(m, realaddr, 4))) ||
      (savedaddr && !(saved = (u8 *)SchlepW(m, savedaddr, 4))) ||
      (effectiveaddr && !(effective = (u8 *)SchlepW(m, effectiveaddr, 4))))
    return -1;
  if (PortableBoxGuestRoot()) {
    gid = m->system->guest_rgid;
    egid = m->system->guest_egid;
    sgid = m->system->guest_sgid;
  } else {
#ifdef HAVE_SETRESUID
    if (getresgid(&gid, &egid, &sgid) == -1) return -1;
#else
    gid = getgid();
    egid = getegid();
    sgid = egid;
#endif
  }
  if (real) Write32(real, gid);
  if (saved) Write32(saved, sgid);
  if (effective) Write32(effective, egid);
  return 0;
}

'''
s=s[:start]+replacement+s[end:]
s=s.replace('''  if (PortableBoxGuestRoot()) return uid == 0 ? 0 : enosys();''','''  if (PortableBoxGuestRoot()) {
    struct System *s = m->system;
    if (s->guest_euid == 0) return PortableBoxGuestChangeUid(s, uid, uid, uid);
    if ((u32)uid != s->guest_ruid && (u32)uid != s->guest_suid) return eperm();
    s->guest_euid = (u32)uid;
    return 0;
  }''',1)
s=s.replace('''  if (PortableBoxGuestRoot()) return gid == 0 ? 0 : enosys();''','''  if (PortableBoxGuestRoot()) {
    struct System *s = m->system;
    if (s->guest_euid == 0) return PortableBoxGuestChangeGid(s, gid, gid, gid);
    if ((u32)gid != s->guest_rgid && (u32)gid != s->guest_sgid) return eperm();
    s->guest_egid = (u32)gid;
    return 0;
  }''',1)
p.write_text(s)
p=Path('build/blink-src/blink/machine.h');s=p.read_text()
needle='''struct System {
  struct XedMachineMode mode;'''
assert needle in s;s=s.replace(needle,'''struct System {
  // PortableBox guest-only Linux credentials. Windows OS token is unchanged.
  // Shared by guest threads and inherited by forked child process image.
  u32 guest_ruid, guest_euid, guest_suid;
  u32 guest_rgid, guest_egid, guest_sgid;
  u32 guest_groups[32];
  u32 guest_ngroups;
  struct XedMachineMode mode;''',1)
p.write_text(s)
print('Patched',len(original),'->',len(replacement))
