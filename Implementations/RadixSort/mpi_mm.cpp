#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define RANDOM 0
#define SORTED 1
#define REVERSE_SORTED 2
#define NOISE 3

const char* main_cali = "main";
const char* data_init_runtime = "data_init_runtime";
const char* correctness_check = "correctness_check";
const char* comm = "comm";
const char* comm_small = "comm_small";
const char* comm_large = "comm_large";
const char* comp = "comp";
const char* comp_small = "comp_small";
const char* comp_large = "comp_large";

std::vector<int> GenerateArray(size_t size, int inputType, int processorNumber) {
    std::vector<int> arr(size);
    srand(static_cast<unsigned int>(rand()) + processorNumber * 100);

    switch (inputType) {
        case RANDOM:
            for (size_t i = 0; i < size; i++) {
                arr[i] = rand() % 10000;
            }
            break;
        case SORTED:
            for (size_t i = 0; i < size; i++) {
                arr[i] = (i + processorNumber * size) % 10000;
            }
            break;
        case REVERSE_SORTED:
            for (size_t i = size; i > 0; i--) {
                arr[i-1] = ((size - i) + processorNumber * size) % 10000;
            }
            break;
        case NOISE:
            for (size_t i = 0; i < size; i++) {
                arr[i] = (i + processorNumber * size) % 10000;
            }
            for (size_t i = 0; i < size / 100; i++) {
                arr[rand() % size] = rand() % 10000;
            }
            break;
    }

    return arr;
}

void PrintArray(const std::vector<int>& arr, int processorNumber) {
    printf("Processor %d, array: ", processorNumber);
    for (size_t i = 0; i < arr.size(); i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

bool SortedVerification(const std::vector<int>& arr) {
    for (size_t i = 1; i < arr.size(); i++) {
        if (arr[i - 1] > arr[i]) {
            return false;
        }
    }
    return true;
}

void ParallelRadixSort(std::vector<int>& arr, int rank, int numprocs) {
    int global_max;
    int local_max = *std::max_element(arr.begin(), arr.end());
    MPI_Allreduce(&local_max, &global_max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    
    std::vector<int> temp(arr.size());
    int bucket[10], offset[10];
    
    for (int exp = 1; global_max / exp > 0; exp *= 10) {
        std::fill_n(bucket, 10, 0);

        for (size_t i = 0; i < arr.size(); i++) {
            int index = (arr[i] / exp) % 10;
            bucket[index]++;
        }

        offset[0] = 0;
        for (int i = 1; i < 10; i++) {
            offset[i] = offset[i - 1] + bucket[i - 1];
        }

        for (size_t i = 0; i < arr.size(); i++) {
            int index = (arr[i] / exp) % 10;
            temp[offset[index]++] = arr[i];
        }

        arr.swap(temp);  // Swaps the contents of temp and arr efficiently
    }
}

int main(int argc, char* argv[]) {
    size_t sizeOfArray;
    int inputType;

    if (argc == 3) {
        sizeOfArray = static_cast<size_t>(atoll(argv[1]));
        inputType = atoi(argv[2]);
    } else {
        fprintf(stderr, "Usage: %s sizeOfArray inputType\n", argv[0]);
        exit(1);
    }

    MPI_Init(&argc, &argv);
    int rank, numprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    
    cali::ConfigManager mgr;
    mgr.start();
    
    CALI_MARK_BEGIN("main"); //MAIN BEGIN=======================================================
    
    CALI_MARK_BEGIN("data_init_runtime"); //DATA INIT BEGIN=======================================================
    std::vector<int> arr = GenerateArray(sizeOfArray, inputType, rank);


    
    
    // Gather initial arrays at root
    std::vector<int> all_arrays;
    if (rank == 0) {
        all_arrays.resize(sizeOfArray * numprocs);
    }
    CALI_MARK_END("data_init_runtime"); //DATA INIT END=======================================================
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_BEGIN("comm"); //COMM BEGIN=======================================================
    
    CALI_MARK_BEGIN("comm_large"); //COMM LARGE BEGIN=======================================================
    MPI_Gather(arr.data(), sizeOfArray, MPI_INT, all_arrays.data(), sizeOfArray, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large"); //COMM LARGE END=======================================================
    
    CALI_MARK_BEGIN("comm_small"); //COMM SMALL BEGIN=======================================================
    
    // Print initial arrays at root
    // if (rank == 0) {
        // printf("Initial Arrays:\n");
        // Assuming all_arrays is a flattened vector of all elements from all processors
        // for (int i = 0; i < numprocs; i++) {
            // printf("Processor %d, array: ", i);
            // for (size_t j = 0; j < sizeOfArray; j++) {
                // printf("%d ", all_arrays[i * sizeOfArray + j]);
            // }
            // printf("\n");
        // }
    // }
    
    CALI_MARK_END("comm_small"); //COMM SMALL END=======================================================
    
    CALI_MARK_END("comm"); //COMM END=======================================================
    
    
    CALI_MARK_BEGIN("comp"); //COMP BEGIN=======================================================
    
    CALI_MARK_BEGIN("comp_large"); //COMP LARGE BEGIN=======================================================
    ParallelRadixSort(arr, rank, numprocs);
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END("comp_large"); //COMP LARGE END=======================================================
    
    // Gather sorted arrays at root
    CALI_MARK_BEGIN("comp_small"); //COMP SMALL BEGIN=======================================================
    MPI_Gather(arr.data(), sizeOfArray, MPI_INT, all_arrays.data(), sizeOfArray, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comp_small"); //COMP SMALL END=======================================================
    
    CALI_MARK_END("comp"); //COMP END=======================================================
    
    
    if (rank == 0) {   
        // Print sorted arrays at root 
        // printf("Sorted Arrays:\n");
        // for (int i = 0; i < numprocs; i++) {
            // printf("Processor %d, array: ", i);
            // for (size_t j = 0; j < sizeOfArray; j++) {
                // printf("%d ", all_arrays[i * sizeOfArray + j]);
            // }
            // printf("\n");
        // }
        
        // Check correctness of the whole sorted array
        CALI_MARK_BEGIN("correctness_check"); //CORRECTNESS BEGIN=======================================================
        bool isSortedCorrectly = true;
        for (int i = 0; i < numprocs; i++) {
            if (!SortedVerification(std::vector<int>(all_arrays.begin() + i * sizeOfArray, all_arrays.begin() + (i + 1) * sizeOfArray))) {
                isSortedCorrectly = false;
                printf("Array segment %d is not sorted correctly.\n", i);
            }
        }
        
        if (isSortedCorrectly) {
            printf("Array is sorted correctly.\n");
        } else {
            printf("Array is not sorted correctly.\n");
        }
        CALI_MARK_END("correctness_check"); //CORRECTNESS END=======================================================
    
    }
    
    
    CALI_MARK_END("main"); //MAIN END=======================================================
    
    adiak::init(NULL);
    adiak::launchdate();    // Launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "radix"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g., "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", sizeOfArray); // The number of elements in input dataset (1000)
    adiak::value("input_type", inputType); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "Noise")
    adiak::value("num_procs", numprocs); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 19); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "ai"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
    
    
    mgr.stop();
    mgr.flush();
    MPI_Finalize();
    
    return 0;
} 
