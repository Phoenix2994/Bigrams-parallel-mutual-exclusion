#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <omp.h>
#include <cmath>
#include <mutex>

#include "grams_computing.h"

using namespace std;

std::mutex hash_mutex;
static const int num_threads = 10;

unordered_map<string, int> letters_hashtable, words_hashtable;

class Worker {
public:
    Worker(
            const long int *positions,
            const char *buffer, int tid) :
            positions(positions), buffer(buffer), tid(tid) {};

    void operator()() {
        doWork();
    }

protected:
    void doWork() {
        std::string ngram[NGRAM_LENGTH], words_ngram;
        int index = 0, letters_index = 0;
        char next_char, letters_ngram[NGRAM_LENGTH + 1];
        long int start_position = positions[tid];
        long int end_position = positions[tid + 1];
        std::string tmp_string;

        for (int i = start_position; i < end_position; i++) {
            next_char = buffer[i];
            if (GramsComputing::computeWords(next_char, ngram, tmp_string, index)) {
                words_ngram = ngram[0] + " " + ngram[1];
                hash_mutex.lock();
                words_hashtable[words_ngram] += 1;
                hash_mutex.unlock();
                if (!(isalpha(next_char) || isspace(next_char)))
                    index = 0;
                else {
                    index = NGRAM_LENGTH - 1;
                    GramsComputing::shiftArrayOfStrings(ngram);
                }
            }

            if (GramsComputing::computeLetters(next_char, letters_ngram, letters_index)) {
                hash_mutex.lock();
                letters_hashtable[letters_ngram] += 1;
                hash_mutex.unlock();
                letters_ngram[0] = letters_ngram[1];
                letters_index = 1;
            }
        }
    }

private:
    const long int *positions;
    const char *buffer;
    int tid;
};


int main(int argc, char *argv[]) {
    const char *kInputPath = argv[1];
    const char *kOutputPath = argv[2];
    ifstream input, size;
    ofstream output_words, output_letters;
    string line, ngram;
    char next_char;
    char *buffer;
    long int tmp_position;
    long int positions[num_threads + 1];

    //start time
    double start = omp_get_wtime();

    //get file size
    size.open(kInputPath, ios::ate);
    const long int file_size = size.tellg();
    size.close();

    //read file
    buffer = new char[file_size];
    input.open(kInputPath, ios::binary);
    input.read(buffer, file_size);
    input.close();

    std::thread threads[num_threads];

    positions[0] = 0;

    for (int i = 1; i < num_threads + 1; i++) {
        if (i == num_threads)
            positions[i] = file_size - 1;
        else {
            tmp_position = floor((file_size - 1) / num_threads) * i;
            next_char = buffer[tmp_position - 1];
            if (isalpha(next_char) || isspace(next_char)) {
                do {
                    next_char = buffer[tmp_position];
                    tmp_position++;
                } while (isalpha(next_char) || isspace(next_char));
            }
            tmp_position++;
            positions[i] = tmp_position;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        Worker w(positions, buffer, i);
        threads[i] = std::thread(w);
    }

    for (auto &thread : threads)
        thread.join();

    //end time
    double elapsedTime = omp_get_wtime() - start;

    /* PRINT RESULTS */
    output_words.open((std::string) kOutputPath + "parallel_output_words.txt", std::ios::binary);
    output_letters.open((std::string) kOutputPath + "parallel_output_letters.txt", std::ios::binary);


    for (auto map_iterator = words_hashtable.begin(); map_iterator != letters_hashtable.end(); ++map_iterator)
        output_words << map_iterator->first << " : " << map_iterator->second << endl;
    output_words << words_hashtable.size() << " elements found." << endl;

    for (auto &map_iterator : letters_hashtable)
        output_letters << map_iterator.first << " : " << map_iterator.second << endl;
    output_letters << letters_hashtable.size() << " elements found." << endl;

    output_words.close();
    output_letters.close();

    cout << elapsedTime;

    return 0;
}