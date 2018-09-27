#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>



#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

const unsigned long long nano = 1000000000;
const unsigned int maxTotalVCPUS = 8;



int getDomainUsageTotal(virDomainPtr dom, virTypedParameterPtr params)
{
        return virDomainGetCPUStats(dom, params, 2,-1,1,0);
}

int getActiveDomains(virConnectPtr conn, virDomainPtr **domListArr) {

        int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_RUNNING;
        int num_active_doms = virConnectListAllDomains(conn, domListArr, flags);

        return num_active_doms;
}

int getPCPU_TIME(int numcpus, int numDoms, virDomainPtr * doms, unsigned long long * timeArr) {
        virTypedParameterPtr params = calloc(numcpus * 1, sizeof(virTypedParameter));
        for (int i = 0; i < numDoms; i++) {
                virDomainGetCPUStats(doms[i], params,1, 0, numcpus, 0);
                for (int x = 0; x < numcpus; x++) {
                        timeArr[x] += params[x].value.ul; //full load
                        printf("%s\n",params[x].field);
                }
                memset(params, 0, numcpus * sizeof(virTypedParameter));
        }

/*params[domainNum]->field*/

        for(int i = 0; i < numcpus; i++) {
                //    printf("CPU %d: %llu\n",i, timeArr[i]);
        }

        free(params);
}
/*
   int getPCPU_TIME_ALT(int numcpus, int numDoms, virDomainPtr * doms, unsigned long long * timeArr) {
        virVcpuInfoPtr vcpuInfo;
        unsigned char *cpumaps; //for the future, if needed
        size_t cpumaplen = VIR_CPU_MAPLEN(numcpus);

        virVcpuInfoPtr cpuinfo = calloc(numcpus, sizeof(virVcpuInfo));


        for (int i = 0; i < numDoms; i++) {
          virDomainGetVcpus(doms[i], cpuinfo, numcpus,NULL, cpumaplen); //assuming max 8 vcpus per domain



        }




   }
 */






double usage(unsigned long long diff, unsigned long long quantum)
{
        return 100 * ( (double) diff / (double) quantum);
}


int getVCPU_STATS(virDomainPtr *doms, int numDoms, int numcpus, virVcpuInfoPtr vcpuinfo, virDomainStatsRecordPtr * record) {
        unsigned char *cpumaps; //for the future, if needed
        size_t cpumaplen = VIR_CPU_MAPLEN(numcpus);
//vcpuinfo is an array of vcpuinfostructs of size numDoms * numcpus, so max 8 vcpus per domain
//vcpuinfo[domain# * numcpus + vcpu#]=infostruct about that vcpu
        for(int i = 0; i < numDoms; i++) {
                //printf("DOMAIN: %d\n", i);
                unsigned int maxvcpus = record[i]->params[0].value.ui;
                if (virDomainGetVcpus(doms[i], (vcpuinfo+(maxTotalVCPUS *i)), maxvcpus, NULL, 0)<0) {
                        printf("could not get vcpus for domain: %d\n", i);
                        return -1;
                }

                /*  for (int x = 0; x < maxvcpus; x++) {
                    printf("VCPU: %llu PINNED TO: %llu, time: %llu\n", (vcpuinfo+(numcpus *i))[x].number,
                    vcpuinfo[numcpus * i + x].cpu,
                    vcpuinfo[numcpus * i + x].cpuTime
                    );
                   }*/



        }

             printf("SANITY CHECK\n");
              for(int i = 0; i < numDoms; i++) {
                      unsigned int maxvcpus = record[i]->params[0].value.ui;
                      printf("# vcpus for domain %d is %d\n", i, maxvcpus);
                      printf("VCPU INFO FOR DOMAIN----- %d\n", i);
                      for (int x = 0; x < maxvcpus; x++) {
                              printf("VCPU: %d (logical: %d) PINNED TO: %d, time: %llu\n", vcpuinfo[maxTotalVCPUS * i + x].number,x,
                                     vcpuinfo[maxTotalVCPUS * i + x].cpu,
                                     vcpuinfo[maxTotalVCPUS * i + x].cpuTime
                                     );

                      }
              }

        return 0;
}

void printRecord(int numDoms, virDomainStatsRecordPtr * record) {
        //DEBUG PRINT
        for (int i = 0; i < numDoms; i++) {
                printf("STATS FOR DOMAIN: %d----\n", i);

                int x = 0;
                for (virTypedParameterPtr param= record[i]->params; x<record[i]->nparams; param++,x++) {
                        printf("%s: %llu\n",param->field, param->value.ul);
                }
        }


}

void printVCPU_USAGE(unsigned long long * VCPU_ARR, virDomainStatsRecordPtr * record, int numDoms, int numcpus) {
        //DEBUG PRINT
        for (int i = 0; i < numDoms; i++) {
                printf("STATS FOR DOMAIN: %d----\n", i);
                int maxvcpus = record[i]->params[0].value.ul;
                for (int x = 0; x<maxvcpus; x++) {
                        printf("VCPU: %d USAGE: %llu\n",x, VCPU_ARR[i*maxTotalVCPUS +x]);
                }
        }


}

int getVCPU_USAGE(unsigned long long * VCPU_ARR, int numDoms, int numcpus, virVcpuInfoPtr prev, virVcpuInfoPtr cur, virDomainStatsRecordPtr* record, int quantum) {
        for (int i = 0; i < numDoms; i++) {
                int maxvcpus = record[i]->params[0].value.ul;
                for (int x = 0; x < maxvcpus; x++) {
                        //  printf("VCPU TIME DIFF %llu\n", (cur[i*numcpus+x].cpuTime-prev[i*numcpus+x].cpuTime));


                        VCPU_ARR[maxTotalVCPUS * i + x]=usage(cur[i*maxTotalVCPUS+x].cpuTime-prev[i*maxTotalVCPUS+x].cpuTime, (unsigned long long)quantum * nano);
                }
        }
}

void getBusFreeCPU(virDomainPtr * doms, unsigned long long * VCPU_USAGE, virVcpuInfoPtr cur, virDomainStatsRecordPtr *record,
  int numDoms, int numcpus, int *busfree, unsigned long long *usageTotal, unsigned long long *vcpusPerCPU, unsigned long * domMap) {

        //unsigned long long usageTotal[numcpus];
        //unsigned long long vcpusPerCPU[numcpus];
        memset(vcpusPerCPU, 0, numcpus * sizeof(unsigned long long));
        memset(usageTotal, 0, numcpus * sizeof(unsigned long long));
        for (int i = 0; i < numDoms; i++) {
                int maxvcpus = record[i]->params[0].value.ul;



                for (int x = 0; x < maxvcpus; x++) {
                        //  printf("VCPU TIME DIFF %llu\n", (cur[i*numcpus+x].cpuTime-prev[i*numcpus+x].cpuTime));
                        usageTotal[cur[i * maxTotalVCPUS + x].cpu]+=VCPU_USAGE[i * maxTotalVCPUS + x];
                  //        printf("CPU: %d adding: %d\n", cur[i * numcpus + x].cpu, VCPU_USAGE[i * numcpus + x]);
                        vcpusPerCPU[cur[i * maxTotalVCPUS + x].cpu]+=1;
                        //VCPU_DOMMAP[cpu# * maxVCPUsPerCPU + vcpu#]=domain # of that vcpu
                        //here, the second maxTotalVCPUS technically means a cpu can have no more than maxTotalVCPUS associated with it
                        domMap[cur[i * maxTotalVCPUS + x].cpu * maxTotalVCPUS + x]=i;
                }

        }
          printf("CPU: %d total: %llu\n", 0, usageTotal[0]);
        double busiest = 0.0;
        double freest = 105.0; //5 is since readings don't happen instantly
        for (int i = 0; i < numcpus; i++) {



              if (vcpusPerCPU[i] == 0) {
                freest = 0;
                busfree[1] = i;
                continue;
              }
              printf("CPU: %d TOTAL USAGE: %f\n", i, (double)usageTotal[i]); // / (double)vcpusPerCPU[i]

                double z =((double)usageTotal[i]); // / (double)vcpusPerCPU[i]


                if(z> busiest) {
                        busiest= (double)usageTotal[i]; // / (double)vcpusPerCPU[i]
                        busfree[0] = i;
                } if (((double)usageTotal[i] /* / (double)vcpusPerCPU[i]*/)< freest) {
                  //printf("FREEST\n");
                        freest= (double)usageTotal[i] /*/ (double)vcpusPerCPU[i]*/;
                        busfree[1] = i;
                }
        }

}

void getBusFreeVCPU(int cpu, int numcpus, int numDoms, virDomainStatsRecordPtr * record, unsigned long long * VCPU_ARR, virVcpuInfoPtr cur, int* result) {
  unsigned long long busiest = 0;
  unsigned long long freest = 100;


  int found = 0;

  for (int i = 0; i < numDoms; i++) {
    int maxvcpus = record[i]->params[0].value.ul;
    for (int x = 0; x < maxvcpus; x++) {

  //    domMap[i * numcpus + x] = doms[i]; //for each cpu, put a ptr to its domain

      if (cur[i * maxTotalVCPUS + x].cpu == cpu) {

        if (VCPU_ARR[i * maxTotalVCPUS +x] > busiest) {
          busiest = max(busiest, VCPU_ARR[i * maxTotalVCPUS + x]);
          result[0] = x;
          result[2]=i;
        }
        if (VCPU_ARR[i *numcpus + x] < freest) {
          freest = min(freest, VCPU_ARR[i * maxTotalVCPUS + x]);
          result[1] = x;
          result[3] = i;
        }


        found = 1;
      }
    }
  }
  if(!found) {
    result[0] = -1;
    result[1] = -1;
  }

}


void VCPU_STAT_CLEAR_AND_MOVE(int numDoms, int numcpus, unsigned long long * VCPU_ARR, virVcpuInfoPtr prev, virVcpuInfoPtr cur) {
        memset(VCPU_ARR, 0, sizeof(unsigned long long) * maxTotalVCPUS*numDoms);
        memcpy(prev, cur, sizeof(virVcpuInfo) * maxTotalVCPUS * numDoms);
        memset(cur, 0, sizeof(virVcpuInfo) * maxTotalVCPUS * numDoms);


}

unsigned long long cpuUse(int cpu, unsigned long long *total, unsigned long long *num) {
  return num[cpu] == 0 ? 0 : total[cpu];
}



//--------------------------------------------------------------------------------------------------------
int
main(int argc, char *argv[])
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
                printf("usage: ./vcpu_scheduler <quantum>");
                return 0;
        }
        int quantum = atoi(argv[1]);
        printf("==PERIOD: %d sec \n", quantum);
/*=====================START MAIN LOOP*/
        virDomainPtr *doms = calloc(10, sizeof(virDomainPtr));  //list of virDomainPtrs of all active domains
        int numDoms = getActiveDomains(conn, &doms);
        if (numDoms == 0) {
                printf("no active domains! \n");
                return -1;
        }
        printf("==ACTIVE DOMAINS: %d\n", numDoms);

        int numcpus = virNodeGetCPUMap(conn, NULL, NULL, 0); //total cpus on host machine
        //numcpus = 4; //to simulate a cpu cap
        printf("==PCPUS: %d\n", numcpus);






/*DEFAULT PINNING INITIALIZATION*/
/*============================INITIAL PINNINGS*/




        int i = 0;


        unsigned long long *PCPU_PREV_TIME = calloc(maxTotalVCPUS, sizeof(unsigned long long));
        unsigned long long *PCPU_CUR_TIME = calloc(maxTotalVCPUS, sizeof(unsigned long long));

        unsigned long long *VCPU_PREV_TIME = calloc(numDoms * maxTotalVCPUS, sizeof(unsigned long long));
        unsigned long long *VCPU_CUR_TIME = calloc(numDoms * maxTotalVCPUS, sizeof(unsigned long long));

        unsigned long long *VCPU_USAGE = calloc(numDoms * maxTotalVCPUS, sizeof(unsigned long long));
        virVcpuInfoPtr vcpuinfoPrev = calloc(numDoms * maxTotalVCPUS, sizeof(virVcpuInfo));
        virVcpuInfoPtr vcpuinfoCur = calloc(numDoms * maxTotalVCPUS, sizeof(virVcpuInfo));

        unsigned long * VCPU_DOMMAP = calloc(maxTotalVCPUS * maxTotalVCPUS, sizeof(unsigned long));


        unsigned long long usageTotal[numcpus];
        unsigned long long vcpusPerCPU[numcpus];

        size_t cpumaplen = VIR_CPU_MAPLEN(numcpus);
        virDomainStatsRecordPtr *record = NULL;
        virDomainListGetStats(doms,VIR_DOMAIN_STATS_VCPU,&record, 0);
        /*initial repin: no pin changes, it just keeps the pins constant */
        getVCPU_STATS(doms, numDoms, numcpus, vcpuinfoCur, record);
        printf("INITIAL REPIN TO DISABLE AUTO BALANCING LIBVIRT\n");
        for (int i = 0; i < numDoms; i++) {
          for (int x = 0; x < record[i]->params[0].value.ul; x++) {
            unsigned char map = 0x1 << vcpuinfoCur[i * maxTotalVCPUS + x].cpu;

               if(virDomainPinVcpu(doms[i], x, &map, cpumaplen) == -1) {
                    printf("pinning failed!\n");
               }
               printf("pinned VCPU %d to PCPU %d\n", x, vcpuinfoCur[i * maxTotalVCPUS + x].cpu);

          }
        }
        free(record); //

/*===================MAIN LOOP START*/
        while(getActiveDomains(conn, &doms)>0) {
                numDoms = getActiveDomains(conn, &doms);
                /*get total domain cpu information*/
                printf("ACTIVE DOMAINS: %d\n", numDoms);

//record[Domain#]->params[0-(2+record[domain#]->params[0].val.ul * 3)].field/val.ul
//nparams for each domain is 2+(numVcpus for that domain * 3)
//or just nparams is record[domain#]->nparams
/*record printout for domain entry w 2 vcpus
   [0]vcpu.current: 2
   [1]vcpu.maximum: 2
   [2]vcpu.0.state: 1
   [3]vcpu.0.time: 22130000000
   [4]vcpu.0.wait: 0
   [5]vcpu.1.state: 1
   [6]vcpu.1.time: 14260000000
   [7]vcpu.1.wait: 0
   numvcpus
 */

                virDomainStatsRecordPtr * record = NULL;

                int numStats = virDomainListGetStats(doms,VIR_DOMAIN_STATS_VCPU,&record, 0);

                //vcpuinfo[domain# * numcpus + vcpu#]=vcpu# info, elements: .cpu(indexed from 0), .number, .cpuTime
                getVCPU_STATS(doms, numDoms, numcpus, vcpuinfoCur, record);

                /*now that we have all stats we technically need, we get usage */

                getVCPU_USAGE(VCPU_USAGE,numDoms,numcpus, vcpuinfoPrev, vcpuinfoCur, record, quantum);



                printVCPU_USAGE(VCPU_USAGE, record, numDoms, numcpus);
                //busfree[0] = busy, busfree[1]=free
                int busfree[2];
                getBusFreeCPU(doms, VCPU_USAGE, vcpuinfoCur, record, numDoms, numcpus, busfree,usageTotal, vcpusPerCPU, VCPU_DOMMAP);
                printf("------------------------\n");
                printf("BUSY: %d, FREE: %d\n", busfree[0], busfree[1]);
                int BUSY_Vbusfree[4];
                //Vbus_free[0] = busy,  [1]=free,[2]=dom busy, [3] dom free
                int FREE_Vbusfree[4];
                getBusFreeVCPU(busfree[0], numcpus, numDoms, record, VCPU_USAGE, vcpuinfoCur, BUSY_Vbusfree);
                getBusFreeVCPU(busfree[1], numcpus, numDoms, record, VCPU_USAGE, vcpuinfoCur, FREE_Vbusfree);
                printf("BUSY CPU: busiest VCPU: %d, freest VCPU %d\n", BUSY_Vbusfree[0], BUSY_Vbusfree[1]);

                printf("FREE CPU: busiest VCPU: %d, freest VCPU %d\n", FREE_Vbusfree[0], FREE_Vbusfree[1]);


                if (cpuUse(busfree[1], usageTotal, vcpusPerCPU) > 50) { //if all active cpus(or freest cpu) have usage above thresh(50%), can't do anything
                  //do nothing
                  printf("NO REPINNING, ALL CPUS ABOVE THRESHOLD!\n");
                  goto cont;
                }
                //busiest cpu must be above thresh to do anything also it must have more than 1 vcpu attached to it

                for (int i = 0; i < numcpus; i++) {
                  if (vcpusPerCPU[i] > 1 && vcpusPerCPU[busfree[1]] == 0) {
                    unsigned char map = 0x1 << busfree[1];
                    //take
                    if(virDomainPinVcpu(doms[VCPU_DOMMAP[i * maxTotalVCPUS + 0]], 0, &map, cpumaplen) == -1) {
                         printf("pinning failed!\n");
                         goto cont;
                    }
                    printf("PINNED VCPU %d of DOM %lu to PCPU %d\n",0,VCPU_DOMMAP[i * maxTotalVCPUS + 0], busfree[1]);
                    goto cont; //if busiest has more than 1 vcpu and freest has none, then no point in checking other conditions
                  }
                }


                if (cpuUse(busfree[0], usageTotal, vcpusPerCPU)>50 && vcpusPerCPU[busfree[0]]>1) {

                  printf("DETECTED IMBALANCE\n");

//only works right now if all cpus are consecutively operating e.g. 0-7, not 0-1, 4-7

                  //REST OF ALGO
                  //else we just take smallest vcpu on busiest and switch it with smallest on freest?

                  unsigned char map = 0x1 << busfree[1]; //freest cpu
                  //pin freest cpu to freest vcpu on busiest cpu
                  if(virDomainPinVcpu(doms[BUSY_Vbusfree[3]], BUSY_Vbusfree[1], &map, cpumaplen) == -1) {
                       printf("pinning failed!\n");
                       goto cont;
                  }
                  printf("PINNED VCPU %d(freest vcpu on busiest cpu) of DOM %d to PCPU(freest) %d\n",BUSY_Vbusfree[1],BUSY_Vbusfree[3], busfree[1]);
                  map = 0x1 << busfree[0];
                  //this swap is only needed on heavy load?
                  //pin busiest cpu to freest vcpu on freest cpu
              /*    if(virDomainPinVcpu(doms[FREE_Vbusfree[3]], FREE_Vbusfree[1], &map, cpumaplen) == -1) {
                       printf("pinning failed!\n");
                       goto cont;
                  }
                  printf("PINNED VCPU %d(freest vcpu on freest cpu) of DOM %d to PCPU(busiest) %d\n",FREE_Vbusfree[1],FREE_Vbusfree[3], busfree[0]);
                  */

                }


                /*

                if there is a
                */
                //PIN VCPUS HERE
                //get busiest vCPU
                //i think each domain will have 1 vcpu and a cpu can have up to 8? vcpus pinned to it?

                goto cont;

//ASK TA about initial affinity/mappings and ask about testcases


cont:
                memset(PCPU_PREV_TIME, 0, sizeof(unsigned long long) * maxTotalVCPUS);
                memcpy(PCPU_PREV_TIME, PCPU_CUR_TIME, sizeof(unsigned long long) * maxTotalVCPUS);
                memset(PCPU_CUR_TIME, 0, sizeof(unsigned long long) * maxTotalVCPUS);

                memset(VCPU_PREV_TIME, 0, sizeof(unsigned long long) * maxTotalVCPUS*numDoms);
                memcpy(VCPU_PREV_TIME, VCPU_CUR_TIME, sizeof(unsigned long long) * maxTotalVCPUS * numDoms);
                memset(VCPU_CUR_TIME, 0, sizeof(unsigned long long) * maxTotalVCPUS * numDoms);

                free(record);
                VCPU_STAT_CLEAR_AND_MOVE(numDoms, maxTotalVCPUS, VCPU_USAGE, vcpuinfoPrev, vcpuinfoCur);
                memset(VCPU_DOMMAP, 0, sizeof(unsigned long) * maxTotalVCPUS * maxTotalVCPUS);


                memset(usageTotal, 0, sizeof(unsigned long long ) * numcpus);
                memset(vcpusPerCPU, 0, sizeof(unsigned long long ) * numcpus);


                printf("======================sleeping for %d sec\n",quantum );
                fflush(stdout);
                sleep(quantum);
        }

        free(doms);





disconnect:
        if (0 != virConnectClose(conn)) {
                /* printf("Failed to disconnect from hypervisor: %s\n",
                         virGetLastErrorMessage());*/

        } else {
                //      printf("Disconnected from hypervisor\n");
        }
}
