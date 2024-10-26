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


void generate_random_array(std::vector<int>& array, int total_elements) {
    array.resize(total_elements);
    for (int i = 0; i < total_elements; ++i) {
        array[i] = rand() % 100;  // Random numbers between 0 and 99
    }
}

void generate_sorted_array(std::vector<int>& array, int total_elements) {
    array.resize(total_elements);
    for (int i = 0; i < total_elements; ++i) {
        array[i] = i;  // Numbers from 0 to total_elements - 1
    }
}

void generate_reverse_sorted_array(std::vector<int>& array, int total_elements) {
    array.resize(total_elements);
    for (int i = 0; i < total_elements; ++i) {
        array[i] = total_elements - 1 - i;  // Numbers from total_elements - 1 down to 0
    }
}

void generate_perturbed_array(std::vector<int>& array, int total_elements) {
    generate_sorted_array(array, total_elements);  // Start with a sorted array
    int perturb_count = total_elements / 100;      // 1% of total elements

    for (int i = 0; i < perturb_count; ++i) {
        int index1 = rand() % total_elements;
        int index2 = rand() % total_elements;
        std::swap(array[index1], array[index2]);  // Randomly swap two elements
    }
}


int main(int argc, char** argv) {
    

    
    // caliper
    CALI_CXX_MARK_FUNCTION;
    /* Define Caliper region names */
    //const char* main = "main";
    
    // REQUIRED
    const char* data_init_runtime = "data_init_runtime";
    const char* correctness_check = "correctness_check";
    const char* comm = "comm";
    const char* comp = "comp";

    //const char* MPI_X = "MPI_X";
    //std::cout << "defined caliper variables\n";

    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();


    // MPI Init
    MPI_Init(&argc, &argv);
    int rank, num_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Get the rank of the process
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes); // Get the total number of processes
    //std::cout << "initialized and started MPI, Caliper\n";
    
    // for whole time
    double time = MPI_Wtime();

    // Ensure that an argument is provided: array size
    if (argc < 3) {
        std::cout << "argc < 3 received " << argc << " arguments\n";

        MPI_Finalize();
        return 1;
    }

    // Get the array size from command-line argument
    int total_elements = atoi(argv[1]);  // Convert string to integer
    int elements_per_process = total_elements / num_processes; // Elements per process
    //std::cout << "elements, elements per proc " << total_elements << " " << elements_per_process << "\n";
    
    
    // verify array sort type
    std::string sortType = argv[2];
    if ((sortType != "RANDOM") && (sortType != "SORTED") && (sortType != "REVERSED") && (sortType != "PERTURBED")) {
        std::cout << "Sort type not recognized.\n";
        MPI_Finalize();
        return 1;
    }
    

    // Initialize the subarray for each process to store the received portion
    std::vector<int> subarray(elements_per_process);

    if (rank == 0) { // Master process (rank 0)
    
        //std::cout << "Inside master.\n";
        
        std::cout << "Total elements: " << total_elements << " Number of processes: " << num_processes << " Sort Type: " << sortType << std::endl;

        // The array to be sorted
        std::vector<int> array;

        // Fill the array with random numbers for demonstration
        CALI_MARK_BEGIN(data_init_runtime);
        array.resize(total_elements);
        if (sortType == "RANDOM") {
          generate_random_array(array, total_elements);
        }
        else if (sortType == "SORTED") {
          generate_sorted_array(array, total_elements);
        }
        else if (sortType == "REVERSED") {
          generate_reverse_sorted_array(array, total_elements);
        }
        else if (sortType == "PERTURBED") {
          generate_perturbed_array(array, total_elements);
        }
        else {
          std::cout << "DEBUG NEEDED" << std::endl;
        }
        CALI_MARK_END(data_init_runtime);
        //std::cout << "Initial array: ";
        //print_array(array, rank);

        // Scatter data from the root process (rank 0) to all other processes
        // Root sends its own subarray and also scatters the array to other processes
        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comm);
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

    const char* comp_small_localsort = "comp_small_localsort";
    CALI_MARK_BEGIN(comp_small_localsort);
    std::sort(subarray.begin(), subarray.end());
    CALI_MARK_END(comp_small_localsort);
    //std::cout << "sorted array, proc " << rank << std::endl; 
    //print_array(subarray, rank);


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
        const char* comp_small_samplesort = "comp_small_samplesort";
        CALI_MARK_BEGIN(comp_small_samplesort);
        std::sort(samples.begin(), samples.end());
        splitters = samples;  // Treat the sorted samples as splitters
        CALI_MARK_END(comp_small_samplesort);
        //std::cout << "Splitters: ";
        //print_array(splitters, rank);
    }
    
    

    // Broadcast the splitters to all processes (master and workers)
    MPI_Bcast(splitters.data(), num_processes, MPI_INT, 0, MPI_COMM_WORLD);
    //std::cout << "broadcasted splitters to all procs\n";
    const char* comp_small_buildbucket = "comp_small_buildbucket";
    CALI_MARK_BEGIN(comp_small_buildbucket);
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
    CALI_MARK_END(comp_small_buildbucket);

    // Root gathers the buckets from worker processes and merges them
    if (rank == 0) {
        std::vector<int> all_buckets;
        all_buckets.insert(all_buckets.end(), bucket.begin(), bucket.end());  // Root adds its own bucket first
        //std::cout << "root added root bucket\n";
        
        const char* comm_large_recvbuckets = "comm_large_recvbuckets";
        CALI_MARK_BEGIN(comm_large_recvbuckets);

        for (int i = 1; i < num_processes; ++i) {
            std::vector<int> temp_bucket(elements_per_process);
            MPI_Status status;
            MPI_Recv(temp_bucket.data(), elements_per_process, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
            all_buckets.insert(all_buckets.end(), temp_bucket.begin(), temp_bucket.end());
        }
        //std::cout << "received buckets\n";
        CALI_MARK_END(comm_large_recvbuckets);
        
        CALI_MARK_END(comm);

        // Sort and print the final sorted array

        const char* comp_small_finalsort = "comp_small_finalsort";
        CALI_MARK_BEGIN(comp_small_finalsort);
        std::sort(all_buckets.begin(), all_buckets.end());
        CALI_MARK_END(comp_small_finalsort);
        //std::cout << "sorted and printed final array\n";
        CALI_MARK_END(comp);

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
        time = MPI_Wtime() - time;
        printf("Total time to run: %f \n", time);

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
    

    

    MPI_Finalize();
    return 0;
}
