#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
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

int* GenerateArray(int size, int inputType, int processorNumber, int totalProcesses) {
    int* arr = new int[size];

    // Seed the random number generator for better randomness
    srand(static_cast<unsigned int>(rand()) + processorNumber); 

    switch (inputType) {
        case RANDOM:
            // Generate random integers between 0 and 9999
            for (int i = 0; i < size; i++) {
                arr[i] = rand() % 10000;
            }
            break;

        case SORTED:
            // Generate sorted integers with an offset for processor ID
            for (int i = 0; i < size; i++) {
                arr[i] = i + (size * processorNumber); // Ensures distinct sorted arrays per processor
            }
            break;

        case REVERSE_SORTED:
            // Generate reverse sorted integers with an offset for processor ID
            for (int i = 0; i < size; i++) {
                arr[i] = (size - i - 1) + (size * processorNumber); // Distinct reverse sorted arrays
            }
            break;

        case NOISE:
            // Create a sorted array first
            for (int i = 0; i < size; i++) {
                arr[i] = i + (size * processorNumber); 
            }

            // Randomly update 1% of the elements
            for (int i = 0; i < size / 100; ++i) { 
                int index = rand() % size;
                arr[index] = rand() % 10000; // Random value
            }
            break;

        default:
            // Invalid input type; handle error
            printf("Invalid input type specified.\n");
            delete[] arr; // Clean up before returning
            return nullptr;
    }

    return arr;
}

void PrintArray(int* arr, int size, int processorNumber) {
    printf("Processor number %d, array:", processorNumber);
    for (int i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

bool SortedVerification(int* arr, int size) { // Check if an array was sorted correctly
    int max = 0;
    for (int i = 0; i < size; i++) {
        if (arr[i] < max) {
            return false;
        }
        max = arr[i];
    }
    return true;
}

// Radix Sort Function
void radixSort(std::vector<int>& arr) {
    int max_element = *std::max_element(arr.begin(), arr.end());
    for (int exp = 1; max_element / exp > 0; exp *= 10) {
        std::vector<int> output(arr.size());
        int count[10] = {0};
        
        // Count occurrences
        for (int i = 0; i < arr.size(); i++)
            count[(arr[i] / exp) % 10]++;
        
        // Cumulative count
        for (int i = 1; i < 10; i++)
            count[i] += count[i - 1];
        
        // Build output array
        for (int i = arr.size() - 1; i >= 0; i--) {
            output[count[(arr[i] / exp) % 10] - 1] = arr[i];
            count[(arr[i] / exp) % 10]--;
        }
        
        // Copy to original array
        arr = output;
    }
}

int main(int argc, char *argv[]) {
    int sizeOfArray; // This is an input to the code
    int inputType;

    if (argc == 3) {
        sizeOfArray = atoi(argv[1]);
        inputType = atoi(argv[2]);
        if (inputType < 0 || inputType > 3) {
            printf("\n input type [0-3]");
            return 0;
        }
    } else {
        printf("\n arguments [sizeOfArray] [input type]");
        return 0;
    }

    int taskid;
    int numTasks;

    MPI_Status status;

    cali::ConfigManager mgr;
    mgr.start();

    CALI_MARK_BEGIN(main_cali);
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &numTasks);

    CALI_MARK_BEGIN(data_init_runtime);
    int* arr = GenerateArray(sizeOfArray, inputType, taskid, numTasks);
    CALI_MARK_END(data_init_runtime);

    int rank = 0;
    while (rank < numTasks) {
        if (rank == taskid) {
            if (rank == 0) {
                printf("Initializing Arrays\n");
            }
            PrintArray(arr, sizeOfArray, taskid);
            fflush(stdout);
        }
        rank++;
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Radix sort for the local arrays
    std::vector<int> local_arr(arr, arr + sizeOfArray);
    radixSort(local_arr);

    // Make sure all arrays finish sorting 
    MPI_Barrier(MPI_COMM_WORLD);

    CALI_MARK_BEGIN(correctness_check);
    bool arraySorted = true;
    if (!SortedVerification(local_arr.data(), sizeOfArray)) {
        printf("Array not sorted, processor %d\n", taskid);
        arraySorted = false;
    }
    for (int i = 0; i < numTasks; ++i) {
        if (i != 0 && i == taskid) {
            int maxPrevArray;
            MPI_Recv(&maxPrevArray, 1, MPI_INT, taskid - 1, 1, MPI_COMM_WORLD, &status);
            if (maxPrevArray > local_arr[sizeOfArray - 1]) {
                printf("Array not sorted, between processor %d and %d\n", taskid - 1, taskid);
                arraySorted = false;
            }
        }
        if (i != numTasks - 1 && i == taskid) {
            MPI_Send(&local_arr[sizeOfArray - 1], 1, MPI_INT, taskid + 1, 1, MPI_COMM_WORLD);
        }
    }
    if (arraySorted) {
        printf("Array Sorted!\n");
    }
    CALI_MARK_END(correctness_check);
    CALI_MARK_END(main_cali);

    // Print finished arrays
    rank = 0;
    while (rank < numTasks) {
        if (rank == taskid) {
            if (rank == 0) {
                printf("Finished Arrays\n");
            }
            PrintArray(arr, sizeOfArray, taskid);
            fflush(stdout);
        }
        rank++;
        MPI_Barrier(MPI_COMM_WORLD);
    }

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
    adiak::value("num_procs", numTasks); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 19); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "ai"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}
