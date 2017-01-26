#include <string>
using namespace std;

int hashFunc(string key);
void createDelta(istream& oldf, istream& newf, ostream& deltaf);
bool applyDelta(istream& oldf, istream& deltaf, ostream& newf);
