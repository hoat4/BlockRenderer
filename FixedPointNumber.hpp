#pragma once

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

        inline bool operator >= (FixedPointNumber other) {
            return value >= other.value;
        }

        inline bool operator > (FixedPointNumber other) {
            return value > other.value;
        }

        inline bool operator < (FixedPointNumber other) {
            return value < other.value;
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
};
