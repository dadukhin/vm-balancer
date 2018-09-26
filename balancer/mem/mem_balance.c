#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>


int getActiveDomains(virConnectPtr conn, virDomainPtr **domListArr)
{
        int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_RUNNING;
        int num_active_doms = virConnectListAllDomains(conn, domListArr, flags);

        return num_active_doms;
}



void getDomainMemoryStats(virDomainPtr *doms, int numDoms, unsigned long long *stats_arr)
{
  for(int i  = 0; i < numDoms; i++) {
    unsigned int nr_stats;
    unsigned int flags = VIR_DOMAIN_AFFECT_CURRENT;
    virDomainMemoryStatStruct tmpStats[VIR_DOMAIN_MEMORY_STAT_NR];
    virDomainSetMemoryStatsPeriod(doms[i], 1, flags); //most accurrate reading
    nr_stats = virDomainMemoryStats(doms[i], tmpStats,	VIR_DOMAIN_MEMORY_STAT_NR, 0);
    printf("DOMAIN: %d: %s MEM STATS: \n", i, virDomainGetName(doms[i]));
    for (int x = 0; x < nr_stats; x++) {
      //printf("HERE!");
      if (//tmpStats[x].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED ||
      //tmpStats[x].tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE ||
      tmpStats[x].tag == VIR_DOMAIN_MEMORY_STAT_USABLE ||
      tmpStats[x].tag == VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON) {
        printf("---%d: %llu\n", tmpStats[x].tag, tmpStats[x].val/1024);
      }

      stats_arr[i * VIR_DOMAIN_MEMORY_STAT_NR + (unsigned int)tmpStats[x].tag] = tmpStats[x].val; //in kb
    //  printf("DOM: %d, INDEX: %d\n", i, (i * VIR_DOMAIN_MEMORY_STAT_NR + (unsigned int)tmpStats[x].tag));
    }




  }

/*
stats_arr[dom# * numdoms + x]
x:
0->current balloon value in kb(6)
1->total data read from swap(0)
2->total memory written to swap(1)
3-># major page faults(2)  //eh
4-># minor page faults(3) //eh
5->total amt of memory left unused by the system?(4)
6->total amt of usable memory for domain(5)
7->how much balloon can be inflated without making guest swap(8)(MOST IMPORTANT STAT)
8->timestamp of last update of stats in seconds(9)
9->resident set size of process running domain(7)
//10->amt of memory that can be quickly reclaimed witout io(for cache disk stuff)(9)
*/


}

 unsigned long long getHostMemoryStats(virConnectPtr conn)
{
	int nparams = 0;
	virNodeMemoryStatsPtr stats = malloc(sizeof(virNodeMemoryStats));
  unsigned long long avail = 0;
	if (virNodeGetMemoryStats(conn,VIR_NODE_MEMORY_STATS_ALL_CELLS, NULL,&nparams, 0) == 0 && nparams != 0) {
		stats = malloc(sizeof(virNodeMemoryStats) * nparams);
		memset(stats, 0, sizeof(virNodeMemoryStats) * nparams);
		virNodeGetMemoryStats(conn,VIR_NODE_MEMORY_STATS_ALL_CELLS,stats,&nparams,0);
	}

  avail = stats[1].value;
  printf("HOST FREE MEM: %llu mb\n", avail/1024);
  free(stats);
  return avail;


}


int main(int argc, char *argv[])
{
/*====================ESTABLISH CONNECTION TO HYPERVISOR*/
        virConnectPtr conn;
        printf("Attempting to connect to hypervisor\n");
        conn=virConnectOpen("qemu:///system");
        if (conn == NULL) {
                printf("could not connect to hv");
                return -1;
        }
        printf("Connected to hypervisor! \n");
/*===================READ INPUT*/
        if (argc!=2) {
                printf("usage: ./memory_coordinator <quantum>");
                return 0;
        }
        int quantum = atoi(argv[1]);
        printf("PERIOD: %d sec \n", quantum);
/*=====================START MAIN LOOP*/
        virDomainPtr *doms = calloc(10, sizeof(virDomainPtr));  //list of virDomainPtrs of all active domains
        int numDoms = getActiveDomains(conn, &doms);
        if (numDoms == 0) {
                printf("no active domains\n");
                return -1;
        }
        printf("ACTIVE DOMAINS: %d\n", numDoms);

        int numcpus = virNodeGetCPUMap(conn, NULL, NULL, 0); //total cpus on host machine
        printf("CPUS: %d\n", numcpus);




        unsigned long long * DOMAIN_MEM_INFO = calloc(numDoms * VIR_DOMAIN_MEMORY_STAT_NR , sizeof(unsigned long long)); //13 stats in each struct
        unsigned long long HOST_MEM_FREE = getHostMemoryStats(conn);

        unsigned long long THRESH_STARVE=100 * 1024;
        unsigned long long THRESH_FREE=200 * 1024;
        unsigned long long factor = 2;
        unsigned long long factorGive = 1;
unsigned int flags = VIR_DOMAIN_AFFECT_LIVE;
//virDomainSetMemoryFlags(doms[0], 985*1024-(THRESH_FREE), flags);


        while((numDoms=getActiveDomains(conn, &doms))>0) {
                /*GET MEMORY STATS*/
                HOST_MEM_FREE = getHostMemoryStats(conn);
          //      printf("ACTIVE DOMS: %d\n", numDoms);
                getDomainMemoryStats(doms, numDoms, DOMAIN_MEM_INFO);

                printf("-------------------ACTIONS:\n");


//unsigned long avail = (unsigned long)DOMAIN_MEM_INFO[numDoms * 0 + 6]; //total available for domain
//              virDomainSetMemoryFlags(doms[0], avail+THRESH_STARVE, flags);
                /*ALGO*/
//for some reason, avail - reclaimed = new balloon
//in virt manager, current allocated mem = libvirt current balloon size?? Y
              int freeDoms[numDoms];
                int starvingDoms[numDoms];
                int starved = 0;
                //unsigned int flags = VIR_DOMAIN_AFFECT_LIVE;
                unsigned long long reclaimed = 0;
                for (int i = 0; i < numDoms; i++) {
                  unsigned long long memFree = DOMAIN_MEM_INFO[VIR_DOMAIN_MEMORY_STAT_NR * i + 8];
                //  printf("FREE: %llu\n", memFree/1024);
                  if ( memFree > THRESH_FREE) {
                    unsigned long long avail = (unsigned long long)DOMAIN_MEM_INFO[VIR_DOMAIN_MEMORY_STAT_NR * i + 6]; //total available for domain +6
                    //printf("AVAIL MEM FOR DOM %d, %llu mb\n", i, avail/1024);
                    //printf("%llu %llu\n", avail, memFree-THRESH_FREE);
                    printf("trying: %llu \n", avail-((memFree-THRESH_FREE)/factor));
                    /*sometimes libvirt memory stat collection is not fast enough to update(even with period=1 set)*/
                    if (memFree >= avail) {
                      goto cont;
                    }

                    if (virDomainSetMemoryFlags(doms[i], avail-((memFree-THRESH_FREE)/factor), flags)==-1) {
                      printf("failed to reclaim!\n");
                    } else {
                      printf("set dom %d to new mem: %llu\n", i, avail-((memFree-THRESH_FREE)/factor));
                      reclaimed+=((memFree-THRESH_FREE)/factor);
                    }

                    freeDoms[i] = 1;
                  } else if(memFree < THRESH_STARVE) {
                    starved++;
                  }
                }
                printf("reclaimed: %llu mb\n", reclaimed / 1024);
                for (int i = 0; i < numDoms; i++) {

                  unsigned long long memFree = DOMAIN_MEM_INFO[VIR_DOMAIN_MEMORY_STAT_NR * i + 8];


                  if (memFree < THRESH_STARVE) {

                  }

                  if (freeDoms[i] != 1 && memFree < THRESH_STARVE) {

                    unsigned long long avail = (unsigned long long )DOMAIN_MEM_INFO[VIR_DOMAIN_MEMORY_STAT_NR * i + 6]; //+6
                    if (HOST_MEM_FREE < (THRESH_STARVE + reclaimed/starved) * starved) {
                      printf("not enough host memory to give!\n");
                      //return -1;
                      goto cont;
                    }

                    //printf("AVAIL MEM FOR DOM %d, %llu mb\n", i, avail/1024);
                    if (avail+THRESH_STARVE*factorGive+reclaimed/starved > virDomainGetMaxMemory(doms[i])) {
                      printf("waiting since cannot allocate more than vm max\n");
                      goto cont;
                    }
                    int s = virDomainSetMemoryFlags(doms[i], avail+THRESH_STARVE*factorGive+reclaimed/starved, flags);
                    if (s==-1) {
                      printf("failed to give domain %d, %llu mb\n",i, (THRESH_STARVE*factorGive+reclaimed/starved) /1024);
                      return -1;
                    }
                    printf("gave domain %d, %llu mb\n", i, (THRESH_STARVE*factorGive+reclaimed/starved) /1024);
                    printf("new size %llu\n", avail+THRESH_STARVE*factorGive+reclaimed/starved);
                  }
                }




                cont:
                memset(DOMAIN_MEM_INFO, 0, numDoms * VIR_DOMAIN_MEMORY_STAT_NR * sizeof(unsigned long long));
                fflush(stdout);
                printf("===================sleeping for %d sec\n", quantum);
                sleep(quantum);

        }
}
