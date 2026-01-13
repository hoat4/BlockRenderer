#pragma once

#include <stdint.h>

/**
 * Negative or positive
 */
class FixedPointNumber {
    private:
        int value;
    
        FixedPointNumber(int value) : value(value) {}

    public:
        FixedPointNumber() : value(0) {}

        static inline FixedPointNumber ofInt(int i) {
            return FixedPointNumber(i << 16);
        }
        static inline FixedPointNumber ofIntAndFractional(int integral, int fraction) {
            return FixedPointNumber(integral << 16 | fraction);
        }

        static inline FixedPointNumber zero() {
            return FixedPointNumber(0);
        }

        static inline FixedPointNumber half() {
            return FixedPointNumber(32768);
        }

        static inline FixedPointNumber min(FixedPointNumber a, FixedPointNumber b) {
            return a < b ? a : b;
        }

        static inline FixedPointNumber min() {
            return FixedPointNumber(INT32_MIN);
        }

        static inline FixedPointNumber max() {
            return FixedPointNumber(INT32_MAX);
        }

        static inline FixedPointNumber max(FixedPointNumber a, FixedPointNumber b) {
            return a > b ? a : b;
        }

        inline bool operator == (FixedPointNumber other) {
            return value == other.value;
        }

        inline bool operator != (FixedPointNumber other) {
            return value != other.value;
        }

        inline bool operator <= (FixedPointNumber other) {
            return value <= other.value;
        }

        inline bool operator >= (FixedPointNumber other) {
            return value >= other.value;
        }

        inline bool operator > (FixedPointNumber other) {
            return value > other.value;
        }

        inline bool operator > (int other) {
            return other < -32768 || other <= 32767 && value > other << 16;
        }

        inline bool operator < (FixedPointNumber other) {
            return value < other.value;
        }

        inline bool operator < (int other) {
            return other > 32767 || other >= -32768 && value < other << 16;
        }

        inline FixedPointNumber operator + (FixedPointNumber other) {
            return FixedPointNumber(value + other.value);
        }

        inline FixedPointNumber operator - (FixedPointNumber other) {
            return FixedPointNumber(value - other.value);
        }

        inline FixedPointNumber operator - (int other) {
            return *this - FixedPointNumber::ofInt(other);
        }

        inline void operator += (FixedPointNumber other) {
            value += other.value;
        }

        inline FixedPointNumber operator * (int other) {
            return FixedPointNumber(value * other);
        }

        inline FixedPointNumber operator / (int other) {
            return FixedPointNumber(value / other);
        }

        inline int floor() {
            return value >> 16;
        }

        inline int fractionalPart() {
            return value & 0xFFFF;
        }

        inline int ceil() {
            return value + 65535 >> 16;
        }

        inline FixedPointNumber operator -() {
            return FixedPointNumber(-value);
        }

        inline static FixedPointNumber hypotSq(FixedPointNumber a, 
                                               FixedPointNumber b, 
                                               FixedPointNumber c) {
            int64_t al = a.value, bl = b.value, cl = c.value;
            return (al*al + bl*bl + cl*cl) >> 32;
        }

        inline float toFloat() {
            return ((float) value) / 65536;
        }

        inline static FixedPointNumber ofFloat(float f) {
            return FixedPointNumber((int) (f * 65536.0));
        }
};
