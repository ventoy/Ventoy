#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long UINT64;

int GetHumanReadableGBSize(UINT64 SizeBytes)
{
    int i;
    int Pow2 = 1;
    double Delta;
    double GB = SizeBytes * 1.0 / 1000 / 1000 / 1000;

    for (i = 0; i < 12; i++)
    {
        if (Pow2 > GB)
        {
            Delta = (Pow2 - GB) / Pow2;
        }
        else
        {
            Delta = (GB - Pow2) / Pow2;
        }

        if (Delta < 0.05)
        {
            return Pow2;
        }

        Pow2 <<= 1;
    }

    return (int)GB;
}

int main(int argc, char **argv)
{
    UINT64 value = strtoul(argv[1], NULL, 10);
    
    printf("%d", GetHumanReadableGBSize(value * 512));
    
    return 0;
}
