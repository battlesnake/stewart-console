/* 
 * File:   main.cpp
 * Author: mark
 *
 * Created on 16 December 2013, 14:06
 */

#include <iostream>
#include <istream>
#include <fstream>
#include <memory>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "stewart-platform/types.h"
#include "stewart-platform/vector.h"
#include "stewart-platform/matrix.h"
#include "stewart-platform/parse.h"

using namespace std;

class OPTIMISATION {
public:
	bool enabled = false;
	float weights[6] = {0.01, 0.01, 0.03, 0.2, 1.0, 0.2};
	float jumpsize = 0.1;
	float delta = 0.05;
	int iterations = 1000;
};

CONFIGURATION *loadConfiguration(OPTIMISATION &opts, istream & data)
{
	CONFIGURATION *config = 0;
	char line[100];
	char key[100], value[100];
	int linenum = 0, reallines = 0;
	data.failbit;
	while (!data.eof()) {
		data.getline(line, 100);
		linenum++;
		if (data.failbit && data.gcount() == 100) {
			cerr << "Line #" << linenum << " too long" << endl;
			if (config)
				delete config;
			return 0;
		}
		if (strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		reallines++;
		int c = sscanf(line, "%[^=]=%*[ \t]%s", key, value);
		if (c != 2) {
			cerr << "Failed to parse line #" << linenum << ": " << line << endl;
			continue;
		}

		/* Trim value */
		int valuelen = strlen(value);
		while (valuelen > 0 && (value[valuelen - 1] == ' ' || value[valuelen - 1] == '\t'))
			valuelen--;
		value[valuelen] = 0;

		/* Trim spaces from right of key */
		int keylen = strlen(key);
		while (keylen > 0 && (key[keylen - 1] == ' ' || key[keylen - 1] == '\t'))
			keylen--;
		key[keylen] = 0;

		cerr << "\t{" << key << "} = '" << value << "'" << endl;

		/* Strut count */
		bool sc = strsame(key, "strut count");
		if (sc && reallines == 1) {
			config = new CONFIGURATION(parseInt(value));
		}
		else if (sc && reallines > 1) {
			cerr << "Duplicate entry: 'strut count'" << endl;
			return 0;
		}
		else if (!sc && reallines == 1) {
			cerr << "First entry in configuration must be 'strut count''" << endl;
			return 0;
		}

			/* Strut/arm size */
		else if (strsame(key, "strut length"))
			config->strut_length = parseFloat(value);
		else if (strsame(key, "strut arm"))
			config->strut_arm = parseFloat(value);

			/* Base */
		else if (strsame(key, "base major radius"))
			config->base_rx = parseFloat(value);
		else if (strsame(key, "base minor radius"))
			config->base_ry = parseFloat(value);
		else if (strsame(key, "base shape"))
			config->base_polygon = parseShape(value);

			/* Top */
		else if (strsame(key, "top major radius"))
			config->platform_rx = parseFloat(value);
		else if (strsame(key, "top minor radius"))
			config->platform_ry = parseFloat(value);
		else if (strsame(key, "top shape"))
			config->platform_polygon = parseShape(value);

			/* Displacement */
		else if (strsame(key, "displacement"))
			config->platform_displacement = parseVector(value);
		else if (strsame(key, "displacement right"))
			config->platform_displacement[0] = parseFloat(value);
		else if (strsame(key, "displacement left"))
			config->platform_displacement[0] = -parseFloat(value);
		else if (strsame(key, "displacement up"))
			config->platform_displacement[1] = parseFloat(value);
		else if (strsame(key, "displacement down"))
			config->platform_displacement[1] = -parseFloat(value);
		else if (strsame(key, "displacement away"))
			config->platform_displacement[2] = parseFloat(value);
		else if (strsame(key, "displacement towards"))
			config->platform_displacement[2] = -parseFloat(value);

			/* Orientation */
		else if (strsame(key, "orientation")) {
			VECTOR orient = parseVector(value);
			config->pitch = orient[0];
			config->yaw = orient[1];
			config->roll = orient[2];
		}
		else if (strsame(key, "orientation pitch"))
			config->pitch = parseFloat(value);
		else if (strsame(key, "orientation yaw"))
			config->yaw = parseFloat(value);
		else if (strsame(key, "orientation roll"))
			config->roll = parseFloat(value);

			/* Optimisation */
		else if (strsame(key, "optimisation enabled"))
			opts.enabled = parseBool(value);
		else if (strsame(key, "optimisation iterations"))
			opts.iterations = parseInt(value);
		else if (strsame(key, "optimisation step size"))
			opts.jumpsize = parseFloat(value);
		else if (strsame(key, "optimisation gradient delta"))
			opts.delta = parseFloat(value);
		else if (strsame(key, "optimisation weight"))
			parseArray6(value, opts.weights);
		else if (strsame(key, "optimisation orientation weight"))
			parseArray3(value, opts.weights);
		else if (strsame(key, "optimisation displacement weight"))
			parseArray3(value, opts.weights + 3);

			/* Unknown */
		else {
			cerr << "Unknown parameter: " << key << " with value " << value << endl;
			if (config)
				delete config;
			return 0;
		}
	}
	config->base[0] = VECTOR::unitX() * config->base_rx;
	config->base[1] = VECTOR::unitZ() * config->base_ry;
	config->platform[0] = VECTOR::unitX() * config->platform_rx;
	config->platform[1] = VECTOR::unitZ() * config->platform_ry;
	cerr << endl;
	return config;
}

struct noop {

	void operator () (istream * ptr) const
	{
	}
};

int main(int argc, char** argv)
{
	char* filename = 0;
	shared_ptr<istream> input;
	if (argc == 2) {
		filename = argv[1];
		input.reset(new ifstream(filename));
		cerr << "Configuration from file " << filename << endl;
	}
	else if (argc > 3) {
		cerr << "Too many parameters!  Syntax: " << argv[0] << " [input file]" << endl;
		return 1;
	}
	else {
		input.reset(&cin, noop());
		cerr << "Configuration from STDIN" << endl;
	}
	OPTIMISATION opts;
	CONFIGURATION *config = loadConfiguration(opts, *input.get());
	if (!config) {
		return 1;
	}
	config->configure();
	config->solve();
	if (opts.enabled) {
		config->optimise(opts.weights, opts.jumpsize, opts.iterations, opts.delta);
	}
	/* Motor angles */
	cerr << "Motor angles: " << endl;
	for (int i = 0; i < config->struts; i++) {
		if (i > 0)
			cout << "\t";
		cout << config->s[i].motor_angle;
	}
	cout << endl << endl;
	cerr << "Epsilon: " << endl;
	cout << config->epsilon() << endl << endl;
	cerr << "Displacement: " << endl;
	cout << config->platform_displacement[0] << "\t" << config->platform_displacement[1] << "\t" << config->platform_displacement[2] << endl << endl;
	cerr << "Orientation (pitch, yaw, roll): " << endl;
	cout << config->pitch << "\t" << config->yaw << "\t" << config->roll << endl << endl;
	delete config;
	return 0;
}

