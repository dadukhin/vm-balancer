# vm-balancer
kvm virtual machine load balancer and scheduler



vm-balancer is a virtual machine load balancer for KVM hypervisor(w/QEMU & libvirt)
-It is capable of efficiently balancing virtual machine loads among up to 8 logical cores(so 4 physical for now)
-very helpful for managing images that are quickly spun up and destroyed with no persistence; e.g. cloud images. Potentially offers speedup and host memory savings when run against native qemu auto-ballooning and repin drivers.


VM CPU BALANCER:
-the balancer will dynamically repin vcpus for all active vm domains to physical cpus to ensure load balance.

compile & run:
1. make && sudo ./vcpu_scheduler <quantum>
quantum is the period in seconds to run the vcpu_scheduler and must be greater than zero

operation:

1.    cpu balancer connects to hypervisor session through default uri
2.    starts sleep by quantum loop, initial setup, pinning, turn off auto-balloon, etc(initial pinning is done to disable internal qemu auto repinning)
3.    gather stats and calculate weighted usage for all vcpus and cpus
4.    Find busiest and freest cpus. If freest cpu has no vcpus pinned to it, pin first vcpu in queue currently pinned to busiest cpu and continue loop
5.    else if busiest cpu usage > 50% threshold and freest cpu < 50% threshold AND busiest cpu has > 1 vcpu pinned to it,
      pin freest vcpu from busiest cpu to freest cpu AND pin freest vcpu on freest vcpu to busiest cpu (often freest cpu has freest vcpu usage < busiest's freest vcpu usage)
6. repeat




VM MEM BALANCER:
-attempts to efficiently reclaim memory from guest vms and distribute memory from a host hypervisor pool to starved guest vms.

compile & run:
1. make && sudo ./memory_coordinator <quantum>
quantum is the period in seconds to run the memory coordinator and must be greater than zero

operation:

1.    mem balancer connects to hypervisor session through default uri
2.    starts sleep by quantum loop, initial setup, pinning, turn off auto-balloon, etc
3.    gather stats and calculate weighted usage
4.    for all vms, if a vm's mem usage is above the free threshold(200mb), reclaim the difference / factor where factor is currently 2.
5.    if a vms usage is starved(below starve threshold 100mb) and it has not given memory to hypervisor pool, give THRESH_STARVE*factorGive+reclaimed/starved.
      where factorGive is currently 1, THRESH_STARVE is the starved threshold, reclaimed is total memory reclaimed in that period by the host, starved is the # of starved cpus.
      Essentially, give the domain:  thresh starve bytes(100mb) * 1 + (a share of the reclaimed memory that particular period).
6.    necessary checks for out-of-memory for host
7.    repeat
