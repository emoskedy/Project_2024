#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <string>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define MASTER 0

int main(int argc, char* argv[]) {
    CALI_CXX_MARK_FUNCTION;

    int N;
    std::string arrayType;

    if (argc == 3) {
        N = atoi(argv[1]);
        arrayType = argv[2];
    }
    else {
        printf("\nPlease provide the size of the array\n");
        return 0;
    }

    CALI_MARK_BEGIN("main");
    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();

    int taskID, num_procs;

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &taskID);
    MPI_Comm_size(MPI_COMM_WORLD,&num_procs);

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", N); // The number of elements in input dataset (1000)
    adiak::value("input_type", arrayType); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "19"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
    
    std::vector<int> globalArray(N);

    // Initilize the data based on the array type
    if (taskID == MASTER) {
        printf("Bitonic sort\n");
        printf("Array size: %d\n", N);
        printf("Array type: %s\n", arrayType.c_str());
        printf("Number of processes: %d\n", num_procs);
        
        CALI_MARK_BEGIN("data_init_runtime");
        if (arrayType == "sorted") {
            for (int i = 0; i < N; i++) {
                globalArray[i] = i;
            }
        } 
        else if (arrayType == "random") {
            for (int i = 0; i < N; i++) {
                globalArray[i] = rand() % N;
            }
        } 
        else if (arrayType == "reverse") {
            for (int i = 0; i < N; i++) {
                globalArray[i] = N - i;
            }
        }
        else if (arrayType == "perturbed") {
            for (int i = 0; i < N; i++) {
                globalArray[i] = i;
            }
            for (int i = 0; i < N / 100; i++) {
                int randomIndex = rand() % N;
                globalArray[randomIndex] = rand() % N;
            }
        } 
        CALI_MARK_END("data_init_runtime");
    }


    int localArray_size = N / num_procs;
    std::vector<int> localArray(localArray_size);

    // Scatter the global array to all processes
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(&globalArray[0], localArray_size, MPI_INT, &localArray[0], localArray_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END ("comm_large");
    CALI_MARK_END("comm");
    
    // Sorting sub array
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    std::sort(localArray.begin(), localArray.end());
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    int num_phases = num_procs / 2;
    for (int i = 1; i <= num_phases; i *= 2) {
        for (int step = i; step > 0; step /= 2) {
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_small");
            int partner_rank;
            std::vector<int> partnerArray(localArray_size);
            int step_size = 2 * step;
            // Check whether the process is in the upper half or lower half
            if (taskID % step_size < step) {
                partner_rank = taskID + step;
            } 
            else {
                partner_rank = taskID - step;
            }
            CALI_MARK_END("comp_small");
            CALI_MARK_END("comp");

            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comp_large");
            MPI_Sendrecv(&localArray[0], localArray_size, MPI_INT, partner_rank, 0, &partnerArray[0], localArray_size, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            CALI_MARK_END("comp_large");
            CALI_MARK_END("comm");


            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comm_large");
            bool isIncreasing = (taskID / 2 * i) % 2 == 0;
            std::vector<int> temp(localArray_size);
            // Merge in ascending order
            if ((isIncreasing && taskID < partner_rank) || (!isIncreasing && taskID > partner_rank)) {
                int idx_local = 0;
                int idx_partner = 0;
                for (int j = 0; j < localArray_size; j++) {
                    if (localArray[idx_local] <= partnerArray[idx_partner]) {
                        temp[j] = localArray[idx_local];
                        idx_local++;
                    } 
                    else {
                        temp[j] = partnerArray[idx_partner];
                        idx_partner++;
                    }
                }
                memcpy(&localArray[0], temp.data(), localArray_size * sizeof(int));
            } 
            // Merge in descending order
            else {
                int idx_local = localArray_size - 1;
                int idx_partner = localArray_size - 1;
                for (int j = localArray_size - 1; j >= 0; j--) {
                    if (localArray[idx_local] >= partnerArray[idx_partner]) {
                        temp[j] = localArray[idx_local];
                        idx_local--;
                    } 
                    else {
                        temp[j] = partnerArray[idx_partner];
                        idx_partner--;
                    }
                }
                memcpy(&localArray[0], temp.data(), localArray_size * sizeof(int));
            }
            CALI_MARK_END("comm_large");
            CALI_MARK_END("comp");
        }
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(&localArray[0], localArray_size, MPI_INT, &globalArray[0], localArray_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END ("comm_large");
    CALI_MARK_END("comm");

    // Check correctness
    if (taskID == MASTER) {
        CALI_MARK_BEGIN("correctness_check");
        bool correct = true;
        for (int i = 0; i < N - 1; i++) {
            if (globalArray[i] > globalArray[i+1]) {
                correct = false;
                break;
            }
        }
        if (correct) {
            printf("\nThe array is correctly sorted.\n");
        }
        else {
            printf("\nThe array is not correctly sorted.\n");
        }
        CALI_MARK_END("correctness_check");
    }

    CALI_MARK_END("main");

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    MPI_Finalize();
}