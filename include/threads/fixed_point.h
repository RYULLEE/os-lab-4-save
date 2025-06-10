#include <stdint.h>
#define DECIMAL (1 << 14)

int convert_int(int n);
int convert_fp(int x);
int convert_fp_round(int x);
int add(int x, int y);
int sub(int x, int y);
int add_mix(int x, int n);
int sub_mix(int x, int n);
int mult(int x, int y);
int mult_mix(int x, int y);
int div(int x, int y);
int div_mix(int x, int n);

int convert_int(int n) {
    return n * DECIMAL;
}

int convert_fp(int x) {
    return x / DECIMAL;
}

int convert_fp_round(int x) {
    if (x >= 0)
        return (x + DECIMAL / 2) / DECIMAL;
    else
        return (x - DECIMAL / 2) / DECIMAL;
}

int add(int x, int y) {
    return x + y;
}

int sub(int x, int y) {
    return x - y;
}

int add_mix(int x, int n) {
    return x + n * DECIMAL;
}

int sub_mix(int x, int n) {
    return x - n * DECIMAL;
}

int mult(int x, int y) {
    return ((int64_t)x) * y / DECIMAL;
}

int mult_mix(int x, int n) {
    return x * n;
}

int div(int x, int y) {
    return ((int64_t)x) * DECIMAL / y;
}

int div_mix(int x, int n) {
    return x / n;
}