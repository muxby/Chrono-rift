#include <iostream>
using namespace std;
int main() {
    int x;
    cout << "Enter x: ";
    if (cin >> x) {
        cin.clear();
        cin.ignore(10000, '\n');
    }
    int y;
    cout << "Enter y: ";
    if (cin >> y) {
        cin.clear();
        cin.ignore(10000, '\n');
    }
    cout << "Done: " << x << ", " << y << "\n";
}
