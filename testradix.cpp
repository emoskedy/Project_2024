#include <iostream>
#include <vector>
using namespace std;

// Function to get the maximum value in the array
int getMax(const vector<int>& arr) {
    int max = arr[0];
    for (int num : arr) {
        if (num > max) {
            max = num;
        }
    }
    return max;
}

// Function to perform counting sort based on a specific digit
void countingSort(vector<int>& arr, int n, int exp) {
    vector<int> output(n);  // Output array
    vector<int> count(10, 0);  // Count array for digits (0-9)

    // Count occurrences of each digit
    for (int i = 0; i < n; i++) {
        int index = (arr[i] / exp) % 10;  // Find the digit at exp position
        count[index]++;
    }

    // Change count[i] so that it contains the actual position of
    // this digit in the output array
    for (int i = 1; i < 10; i++) {
        count[i] += count[i - 1];
    }

    // Build the output array
    for (int i = n - 1; i >= 0; i--) {
        int index = (arr[i] / exp) % 10;
        output[count[index] - 1] = arr[i];
        count[index]--;
    }

    // Copy the output array back to the original array
    for (int i = 0; i < n; i++) {
        arr[i] = output[i];
    }
}

// Main function to perform Radix Sort
void radixSort(vector<int>& arr) {
    int n = arr.size();
    int max = getMax(arr);  // Find the maximum number

    // Perform counting sort for each digit (exponentially increasing)
    for (int exp = 1; max / exp > 0; exp *= 10) {
        countingSort(arr, n, exp);
    }
}

// Function to print the array
void printArray(const vector<int>& arr) {
    for (int num : arr) {
        cout << num << " ";
    }
    cout << endl;
}

// Example usage
int main() {
    vector<int> arr = {170, 45, 75, 90, 802, 24, 2, 66};
    
    cout << "Unsorted array: ";
    printArray(arr);

    radixSort(arr);  // Sort the array using Radix Sort

    cout << "Sorted array: ";
    printArray(arr);  // Print the sorted array

    return 0;
}
