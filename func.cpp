#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>
using namespace std;

//========================================================================
// Timer t;                 // create a timer and start it
// t.start();               // start the timer
// double d = t.elapsed();  // milliseconds since timer was last started
//========================================================================

#include <chrono>

class Timer
{
public:
	Timer()
	{
		start();
	}
	void start()
	{
		m_time = std::chrono::high_resolution_clock::now();
	}
	double elapsed() const
	{
		std::chrono::duration<double, std::milli> diff =
			std::chrono::high_resolution_clock::now() - m_time;
		return diff.count();
	}
private:
	std::chrono::high_resolution_clock::time_point m_time;
};

// Hash by the first character of each N-byte sequence
int hashFunc(string key)
{
	if (key.empty())
		return 0;
	return key[0];
}

// Use a struct to store each N-byte sequence and their offsets
struct Items
{
	int m_offset;
	string m_seq;
};

void createDelta(istream& oldf, istream& newf, ostream& deltaf)
{
	//Timer timer;
	//timer.start();

	// Read the content of oldf and newf into strings
	char oldc;
	char newc;
	string olds = "";
	string news = "";
	while (oldf.get(oldc))
		olds += oldc;
	while (newf.get(newc))
		news += newc;

	// Create hash table (vector of vectors) for old file's string
	vector<vector<Items>> table;
	const int numBuckets = 256;		// One bucket for each ASCII character
	table.resize(numBuckets);
	Items s;
	const int N = 8;		// Use N=8 to look for 8-byte sequences
	s.m_offset = 0;
	for (; s.m_offset < olds.size(); (s.m_offset)++)	// Hash each 8-byte sequence in the old file's string by the first character in the sequence
	{
		s.m_seq = olds.substr(s.m_offset, N);
		int index = hashFunc(s.m_seq);
		table[index].push_back(s);
	}
	//// Print hash table (debugging)
	//for (int i = 0; i < 125; i++)
	//{
	//	cerr << '[' << i << "]: ";
	//	for (int j = 0; j < table[i].size(); j++)
	//	{
	//		cerr << table[i][j].m_seq << '(' << table[i][j].m_offset << "); ";
	//	}
	//	cerr << endl;
	//}
	//cerr << "Hash table created" << endl;

	// Loop through newf's string to look for matches in oldf
	string toAdd = "";		// Use a temp string to hold sequences that we can't find in oldf's string
	for (int i = 0; i < news.size(); )
	{
		string newsubs = news.substr(i, N);
		int index = hashFunc(newsubs);
		bool foundMatch = false;
		int numToCopy = newsubs.size();
		int temp_new_i = i + N;		// When we find a match, use temp_new_i to advance past the matching sequence
		int goHereNew = temp_new_i;	// After finding the best matching sequence, advance by this much
		int bestOffset;				// Stores the offset of the best matching sequence
		int bestNumToCopy = 0;		// Stores the length of the best matching sequence
		int j = 0;
		for (; i < news.size() - N + 2 && j < table[index].size(); j++)		// For each N-byte sequence in newf, iterate through the corresponding bucket to look for the best match
		{
			if (newsubs == table[index][j].m_seq)	// Found a match
			{
				numToCopy = newsubs.size();
				foundMatch = true;
				// Check if the match is longer than the N-character sequence
				int temp_old_i = table[index][j].m_offset + N;	// Temp_old_i advances past the N-byte matching sequence so we can see if the match is actually longer than N bytes
				temp_new_i = i + N;		// Temp_new_i also advances so we can look for a longer match
				while ((temp_new_i < news.size()) && (temp_old_i < olds.size()) && (news[temp_new_i] == olds[temp_old_i]))	// Loop to find a longer match
				{
					++numToCopy;
					++temp_new_i;
					++temp_old_i;
				}
				if (numToCopy > bestNumToCopy)	// Keep track of the best match
				{
					bestNumToCopy = numToCopy;
					bestOffset = table[index][j].m_offset;
					goHereNew = temp_new_i;
				}
			}
		}
		if (foundMatch) // Found a match
		{
			if (!toAdd.empty())		// Add characters that we didn't find a match for
			{
				deltaf << 'A' << toAdd.size() << ':' << toAdd;
				toAdd = "";
			}
			deltaf << 'C' << bestNumToCopy << ',' << bestOffset;	// Copy the best match
			i = goHereNew;
		}
		else	// Can't find a match, so we eventually have to add the first character of newf's substring
		{
			toAdd += newsubs[0];
			if (i == news.size() - 1)	// If we've reached the end of the string and haven't found any matches, then it's time to add to the delta file
			{
				deltaf << 'A' << toAdd.size() << ':' << toAdd;
			}
			++i;
		}
	}
	//cerr << "createDelta took " << timer.elapsed() << " milliseconds" << endl;
}

bool applyDelta(istream& oldf, istream& deltaf, ostream& newf)
{
	//Timer timer;
	//timer.elapsed();
	if (!oldf || !deltaf || !newf)
	{
		if (!oldf)
			cerr << "Error: Cannot open old file" << endl;
		else if (!deltaf)
			cerr << "Error: Cannot open delta file" << endl;
		else if (!newf)
			cerr << "Error: Cannot open new file" << endl;
		exit(1);
	}

	char c;
	while (deltaf.get(c))
	{
		if (c == 'A')	// Add command
		{
			// Convert string to int to determine the number of characters to add
			int stoi = 0;
			while (deltaf.get(c) && c != ':')
			{
				stoi = stoi * 10 + c - '0';
			}

			// Add that amount of characters from deltaf to new f
			for (int i = 0; i < stoi; i++)
			{
				if (deltaf.get(c))
					newf << c;
				else
					return false;	// Invalid command: Asks to add more characters than is provided in the delta file
			}
		}

		else if (c == 'C')	// Copy command
		{
			// Convert string to int to determine the number of characters to copy
			int numChars = 0;
			while (deltaf.get(c) && c != ',')
				numChars = numChars * 10 + c - '0';

			// Convert string to int to determine the offset in the old file
			int offset = 0;
			while (deltaf.peek() >= '0' && deltaf.peek() <= '9')
			{
				deltaf.get(c);
				offset = offset * 10 + c - '0';
			}

			if (offset < 0)
				return false;

			// Set the input position of the old file
			oldf.seekg(offset, oldf.beg);

			// Copy characters from old file to new file
			char oldC;
			for (int i = 0; i < numChars; i++)
			{
				if (oldf.get(oldC))
					newf << oldC;
				else
					return false;	// Invalid offset
			}
		}
		else
			return false;	// Found a command that isn't 'A' or 'C'
	}
	//cerr << "applyDelta took " << timer.elapsed() << " milliseconds" << endl;
	return true;
}