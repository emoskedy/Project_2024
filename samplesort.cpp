#include <iostream>
#include <mpi.h>
#include <vector>
#include <algorithm>
#include <cstdlib> 
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <string>

// Helper function to print the contents of an array
void print_array(const std::vector<int>& arr, int rank) {
    std::cout << "Process " << rank << ": ";
    for (auto& el : arr) {
        std::cout << el << " ";
    }
    std::cout << std::endl;
}

bool is_sorted(const std::vector<int>& arr, int& index1, int& index2, int& num1, int& num2) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i] < arr[i - 1]) {
            index1 = i;
            index2 = i - 1;
            num1 = arr[i];
            num2 = arr[i - 1];
            return false;  // If any element is smaller than the previous one, the array is not sorted
        }
    }
    return true;  // If no elements violate the sorting order, the array is sorted
}

int main(int argc, char** argv) {

    // caliper
    CALI_CXX_MARK_FUNCTION;
    /* Define Caliper region names */
    const char* main = "main";
    const char* data_init_runtime = "data_init_runtime";
    const char* correctness_check = "correctness_check";
    const char* comm_small = "comm_small";
    const char* comm_large = "comm_large";
    const char* comp_small = "comp_small";
    const char* comp_large_final = "comp_large_final";
    const char* comp_large_middle = "comp_large_middle";
    //const char* MPI_X = "MPI_X";
    //std::cout << "defined caliper variables\n";

    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();

    CALI_MARK_BEGIN(main);

    // MPI Init
    MPI_Init(&argc, &argv);
    int rank, num_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Get the rank of the process
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes); // Get the total number of processes
    //std::cout << "initialized and started MPI, Caliper\n";

    // Ensure that an argument is provided: array size
    if (argc < 2) {
        std::cout << "argc < 2\n, received " << argc << " arguments\n";

        MPI_Finalize();
        return 1;
    }

    // Get the array size from command-line argument
    int total_elements = atoi(argv[1]);  // Convert string to integer
    int elements_per_process = total_elements / num_processes; // Elements per process
    //std::cout << "elements, elements per proc " << total_elements << " " << elements_per_process << "\n";

    // Initialize the subarray for each process to store the received portion
    std::vector<int> subarray(elements_per_process);

    if (rank == 0) { // Master process (rank 0)
    
        //std::cout << "Inside master.\n";

        // The array to be sorted
        std::vector<int> array;

        // Fill the array with random numbers for demonstration
        CALI_MARK_BEGIN(data_init_runtime);
        array.resize(total_elements);
        for (int i = 0; i < total_elements; ++i) {
            array[i] = rand() % 100;  // Random numbers between 0 and 99
        }
        CALI_MARK_END(data_init_runtime);
        //std::cout << "Initial array: ";
        //print_array(array, rank);

        // Scatter data from the root process (rank 0) to all other processes
        // Root sends its own subarray and also scatters the array to other processes
        CALI_MARK_BEGIN(comm_large);
        MPI_Scatter(array.data(), elements_per_process, MPI_INT, 
                    subarray.data(), elements_per_process, MPI_INT, 
                    0, MPI_COMM_WORLD);
        //std::cout << "scattered data from master\n";

    } else { // Worker processes (rank > 0)

        // Worker processes receive their subarray from the root (rank 0)
        MPI_Scatter(NULL, 0, MPI_DATATYPE_NULL, 
                    subarray.data(), elements_per_process, MPI_INT, 
                    0, MPI_COMM_WORLD);
        //std::cout << "received scatter, proc " << rank << std::endl; 
    }

    // Each process sorts its received subarray locally
    CALI_MARK_BEGIN(comp_large_middle);
    std::sort(subarray.begin(), subarray.end());
    //std::cout << "sorted array, proc " << rank << std::endl; 
    //print_array(subarray, rank);
    CALI_MARK_END(comp_large_middle);

    // Each process selects a sample (for simplicity, we select the middle element of the sorted subarray)
    int sample = subarray[elements_per_process / 2];
    //std::cout << "sample: " << sample << std::endl;

    // Root process gathers all the samples from worker processes
    std::vector<int> samples(num_processes);

    // Root process: Sort the samples and determine splitters, then broadcast them to all processes
    std::vector<int> splitters(num_processes);  // Ensure splitters is properly sized on all processes
    
    // Sort the gathered samples and select splitters
    MPI_Gather(&sample, 1, MPI_INT, samples.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::sort(samples.begin(), samples.end());
        splitters = samples;  // Treat the sorted samples as splitters
        //std::cout << "Splitters: ";
        //print_array(splitters, rank);
    }

    // Broadcast the splitters to all processes (master and workers)
    MPI_Bcast(splitters.data(), num_processes, MPI_INT, 0, MPI_COMM_WORLD);
    //std::cout << "broadcasted splitters to all procs\n";

    // Redistribute data based on splitters: Each process builds a bucket and sends it to the root
    std::vector<int> bucket;
    for (int i = 0; i < elements_per_process; ++i) {
        int elem = subarray[i];
        for (int j = 0; j < num_processes; ++j) {
            if (elem <= splitters[j]) {
                bucket.push_back(elem);
                break;
            }
        }
    }
    //std::cout << "redistributed data based on splitters, rank " << rank << std::endl;

    // Root gathers the buckets from worker processes and merges them
    if (rank == 0) {
        std::vector<int> all_buckets;
        all_buckets.insert(all_buckets.end(), bucket.begin(), bucket.end());  // Root adds its own bucket first
        //std::cout << "root added root bucket\n";

        for (int i = 1; i < num_processes; ++i) {
            std::vector<int> temp_bucket(elements_per_process);
            MPI_Status status;
            MPI_Recv(temp_bucket.data(), elements_per_process, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
            all_buckets.insert(all_buckets.end(), temp_bucket.begin(), temp_bucket.end());
        }
        //std::cout << "received buckets\n";
        CALI_MARK_END(comm_large);

        // Sort and print the final sorted array
        CALI_MARK_BEGIN(comp_large_final);
        std::sort(all_buckets.begin(), all_buckets.end());
        //std::cout << "sorted and printed final array\n";
        CALI_MARK_END(comp_large_final);

        // check correctness
        CALI_MARK_BEGIN(correctness_check);
        int index1 = -1;
        int index2 = -1;
        int num1 = -1;
        int num2 = -1;
        if (is_sorted(all_buckets, index1, index2, num1, num2)) {
            //std::cout << "Final sorted array: ";
            //print_array(all_buckets, rank);
            std::cout << "array correctly sorted.\n";
        }
        else{
            std::cout << "Array not sorted. " << index1 << " " << index2 << " " << num1 << " " << num2 << std::endl;
        }
        CALI_MARK_END(correctness_check);


        // necessary code

        std::string algorithm = "";
        std::string programming_model = "";
        std::string data_type = "";
        std::string size_of_data_type = "";
        std::string scalability = "";
        std::string group_number = "";
        std::string implementation_source = "";

        adiak::init(NULL);
        adiak::launchdate();    // launch date of the job
        adiak::libraries();     // Libraries used
        adiak::cmdline();       // Command line used to launch the job
        adiak::clustername();   // Name of the cluster
        adiak::value("samplesort", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
        adiak::value("mpi", programming_model); // e.g. "mpi"
        adiak::value("int", data_type); // The datatype of input elements (e.g., double, int, float)
        adiak::value("4", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
        //adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
        //adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
        adiak::value("num_procs", num_processes); // The number of processors (MPI ranks)
        adiak::value("strong", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
        adiak::value("19", group_number); // The number of your group (integer, e.g., 1, 10)
        adiak::value("handwritten, with ai for consult", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    } else { // Worker processes (rank > 0)
        // Send the bucket to the root process
        MPI_Send(bucket.data(), bucket.size(), MPI_INT, 0, 0, MPI_COMM_WORLD);
        //std::cout << rank << " proc sent bucket to root\n";
    }


    CALI_MARK_END(main);
    MPI_Finalize();
    return 0;
}
