#include <mpi.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <ctime>

std::vector<int> generateArray(int n) {
    std::vector<int> arr(n);
    srand(time(0));
    for (int &x : arr) {
        x = rand() % 100; // Random numbers between 0 and 99
    }
    return arr;
}

std::vector<int> distributeArray(const std::vector<int> &A, int n, int num_procs, int rank) {
    int local_size = n / num_procs;
    std::vector<int> local_arr(local_size);
    MPI_Scatter(A.data(), local_size, MPI_INT, local_arr.data(), local_size, MPI_INT, 0, MPI_COMM_WORLD);
    return local_arr;
}

std::vector<int> selectSample(const std::vector<int> &local_arr, int num_procs) {
    int sample_size = num_procs - 1;
    std::vector<int> sample(sample_size);
    srand(time(0) + MPI_COMM_WORLD); // Ensure different random seeds
    for (int i = 0; i < sample_size; ++i) {
        sample[i] = local_arr[rand() % local_arr.size()];
    }
    std::sort(sample.begin(), sample.end());
    return sample;
}

std::vector<std::vector<int>> partition(const std::vector<int> &local_arr, const std::vector<int> &split, int num_procs) {
    std::vector<std::vector<int>> partitions(num_procs);
    for (int elem : local_arr) {
        int idx = std::upper_bound(split.begin(), split.end(), elem) - split.begin();
        partitions[idx].push_back(elem);
    }
    return partitions;
}

std::vector<int> mergePartitions(const std::vector<std::vector<int>> &partitions) {
    std::vector<int> merged_arr;
    for (const auto &partition : partitions) {
        merged_arr.insert(merged_arr.end(), partition.begin(), partition.end());
    }
    std::sort(merged_arr.begin(), merged_arr.end()); // Simple k-way merge
    return merged_arr;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    std::vector<int> A;
    int n;

    if (rank == 0) {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <number of elements>\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        n = std::atoi(argv[1]);
        A = generateArray(n);
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> local_arr = distributeArray(A, n, num_procs, rank);
    
    std::vector<int> local_sample = selectSample(local_arr, num_procs);
    std::vector<int> all_samples(num_procs * (num_procs - 1));
    
    MPI_Gather(local_sample.data(), num_procs - 1, MPI_INT, all_samples.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> split;
    if (rank == 0) {
        std::sort(all_samples.begin(), all_samples.end());
        for (int i = 1; i < num_procs; ++i) {
            split.push_back(all_samples[i * (num_procs - 1)]);
        }
    }

    split.resize(num_procs - 1);
    MPI_Bcast(split.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<std::vector<int>> partitions = partition(local_arr, split, num_procs);
    std::vector<int> send_counts(num_procs);
    std::vector<int> recv_counts(num_procs);
    std::vector<int> send_displs(num_procs);
    std::vector<int> recv_displs(num_procs);

    std::vector<int> local_partition;
    for (int i = 0; i < num_procs; ++i) {
        send_counts[i] = partitions[i].size();
        local_partition.insert(local_partition.end(), partitions[i].begin(), partitions[i].end());
    }

    MPI_Alltoall(send_counts.data(), 1, MPI_INT, recv_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    int total_recv = 0;
    for (int i = 0; i < num_procs; ++i) {
        recv_displs[i] = total_recv;
        total_recv += recv_counts[i];
    }

    std::vector<int> recv_buffer(total_recv);
    MPI_Alltoallv(local_partition.data(), send_counts.data(), send_displs.data(), MPI_INT,
                  recv_buffer.data(), recv_counts.data(), recv_displs.data(), MPI_INT, MPI_COMM_WORLD);

    std::vector<int> local_sorted_arr = mergePartitions({recv_buffer});
    std::vector<int> sorted_arr;
    if (rank == 0) {
        sorted_arr.resize(n);
    }

    MPI_Gather(local_sorted_arr.data(), local_sorted_arr.size(), MPI_INT, sorted_arr.data(), local_sorted_arr.size(), MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Sorted array: ";
        for (int x : sorted_arr) {
            std::cout << x << " ";
        }
        std::cout << "\n";
    }

    MPI_Finalize();
    return 0;
}