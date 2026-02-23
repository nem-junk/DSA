#include <iostream>
#include <algorithm>

InsertionSort(int array[], int length)
{
  for (int i = 1;1<length;1++)
  {
    int j = i ;
    while (j > 0, && array[j] < array[j-1])
    {
      std::swap (array[j],array[j-1])

      j--;
    }
  }
}


int main()
{
  int data[] = {7, 4, 1, 6, 5};
  int length = sizeof(data) / sizeof(data[0]);
  InsertionSort(data,length);

  return 0;

}
