#include <iostream>
using namespace std;

int main()
{
	cout << "Hello C++" << endl;

	for (int i = 0; i < 10; ++i)
	{
		cout << i << endl;

#ifdef _DEBUG
		if (i == 5)
			cout << "This is a debug msg!" << endl;
#endif // DEBUG

	}
	return 0;
}