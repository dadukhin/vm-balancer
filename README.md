# vm-balancer
kvm virtual machine load balancer and scheduler



vm-balancer is a virtual machine load balancer for KVM hypervisor(w/QEMU & libvirt)
-It is capable of efficiently balancing virtual machine loads among up to 8 logical cores(so 4 physical for now)
-very helpful for managing images that are quickly spun up and destroyed with no persistence; e.g. cloud images. Potentially offers speedup and host memory savings when run against native qemu auto-ballooning and repin drivers.

VM CPU BALANCER:
-the balancer will dynamically repin vcpus for all active vm domains to physical cpus to ensure load balance.

operation:
1. cpu balancer connects to hypervisor session through default uri
2. starts sleep by quantum loop, initial setup, pinning, turn off auto-balloon, etc(initial pinning is done to disable internal qemu auto repinning)
3. gather stats and calculate weighted usage for all vcpus and cpus
4. for all vms, find the busiest cpu and the least busy cpu. if the busiest is above the balance threshold for usage & the freest is below, do the following:
5. pin freest cpu to freest vcpu on busiest cpu, pin busiest cpu to freest vcpu on freest cpu(through repeated iterations, the swaps gradually induce a steady state)
6. if freest cpu has no vcpus, simply pin busiest vcpu on busiest cpu to that cpu
7. repeat


VM MEM BALANCER:
-attempts to efficiently reclaim memory from guest vms and distribute memory from a host hypervisor pool to starved guest vms.

operation:
1. mem balancer connects to hypervisor session through default uri
2. starts sleep by quantum loop, initial setup, pinning, turn off auto-balloon, etc
3. gather stats and calculate weighted usage
4. for all vms, if a vm's mem usage is above the free threshold, reclaim the difference / factor where factor is currently 2.
5. if a vms usage is starved(below starve threshold) and it has not given memory to hypervisor pool, give THRESH_STARVE*factorGive+reclaimed/starved. So, give the domain thresh starve bytes * factor + a share of the reclaimed memory that particular period.
6. necessary checks for out-of-memory for host
7. repeat
