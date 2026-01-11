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
        static inline FixedPointNumber ofInt(int i) {
            return FixedPointNumber(i << 16);
        }
        static inline FixedPointNumber ofIntAndFractional(int integral, int fraction) {
            return FixedPointNumber(integral << 16 | fraction);
        }
        static inline FixedPointNumber zero() {
            return FixedPointNumber(0);
        }

        static inline FixedPointNumber min(FixedPointNumber a, FixedPointNumber b) {
            return a < b ? a : b;
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
};
