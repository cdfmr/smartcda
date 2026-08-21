#include <stdio.h>
#include <string>

#pragma warning(disable: 4996)

FILE *flog = NULL;

void open_log(const std::string &filename)
{
    flog = fopen(filename.c_str(), "a");
}

void close_log()
{
    fclose(flog);
}
