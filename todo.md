# OS Systems-Level TODO

## Critical
- [ ] **Enable Firewall** — UFW is inactive, ports 80, 443, 3030, 7070 exposed on 0.0.0.0 with zero filtering. `sudo ufw enable` + allow only needed ports.
- [ ] **Activate fail2ban** — Installed but inactive. No brute-force protection. `sudo systemctl enable --now fail2ban`
- [ ] **Install smartmontools** — No disk health monitoring on NVMe or SSD. `sudo apt install smartmontools` + enable `smartd`

## High
- [ ] **Set up backups** — No scheduled backups (only rsync present). Install `timeshift` for system snapshots and/or `restic` for data.

## Medium
- [ ] **Cap journald logs** — Currently 3.3 GB and unbounded. Set `SystemMaxUse=500M` in `/etc/systemd/journald.conf`
- [ ] **Reformat 1TB SSD from NTFS** — `/mnt/1tb-ssd` is ntfs-3g (slow FUSE driver, no TRIM). Reformat to ext4/btrfs if no Windows dual-boot need.
- [ ] **Remove cloud-init** — 4 services running at boot for no reason on a desktop. `sudo apt remove cloud-init`

## Low
- [ ] **Add `noatime` to root mount** — Reduce unnecessary write ops in `/etc/fstab`
- [ ] **Lower vm.swappiness** — Set to 10 (from 60) given 64 GB RAM. `sysctl vm.swappiness=10`, persist in `/etc/sysctl.d/`
- [ ] **Disable AnyDesk + Avahi** — Unnecessary attack surface if not actively used. `sudo systemctl disable --now anydesk avahi-daemon`
