/*
 * wav_test.cpp
 *
 * This is a simple test-utility to make sure the WavFile loader
 * works correctly.
 *
 * Copyright (C) 2018, Christopher Collins
 */

#include <iostream>
#include <cstdlib>

#include "WavFile.h"

using namespace std;

int
main(int argc, char **argv)
{
	if (argc <= 1) {
		cerr << "Must specify file to load." << endl;
		exit(1);
	}
	for (int i = 1; i < argc; i++) {
		cout << "------" << endl
			<< "Loading File: " << argv[i] << endl;
		auto data = LoadWav(argv[i]);

		if (nullptr == data) {
			cerr << "Unable to load: " << argv[i] << endl;
			continue;
		}

		cout << "Loaded successfully" << endl;

		cout << "Sampling Rate: " << data->getSampleRate() << endl
			<< "Sample Size: " << (int) data->getBitsPerSample() << endl
			<< "Channels: " << (int) data->getNumChannels() << endl;
	}
	return 0;
}
