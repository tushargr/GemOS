#include<lib.h>

void userapp (int length, int *a)
{
     int i, j;
     char fmt1[4]={'%', 'd', ' ', '\0'}, fmt2[2] = {'\n', '\0'};
     
     i=0;
     j=0;
     i=(j+5)/i; 
     for(i=1; i<length; ++i)
     {
        int element = a[i];
        j=i-1;
        while(j>=0 && a[j] > element){
                a[j+1] = a[j];
                j--;
        }
        a[j+1] = element;
     }
     return;
}
