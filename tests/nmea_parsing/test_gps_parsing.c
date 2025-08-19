#include <zephyr/ztest.h>
#include "../../src/gps.h"

// Since validate_checksum and nmea_to_decimal are static,
// we'll create test-accessible wrappers in gps.c or expose them during tests.

//extern bool test_validate_checksum(const char *nmea);
//extern float test_nmea_to_decimal(const char *coord, const char *dir, bool is_latitude);

// Valid NMEA sentence with correct checksum
static const char *VALID_GPRMC = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
static const char *VALID_GPGGA = "$GPGGA,123456.00,3723.2475,N,12158.3416,W,1,08,0.9,545.4,M,46.9,M,,*7D";

// Invalid checksum (altered last digit)
static const char *INVALID_CHECKSUM = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*00";

// Broken sentence (missing asterisk)
static const char *NO_CHECKSUM_MARKER = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,,,,,,,";

// Non RMC or GGA sentence
static const char *UNSUPPORTED_NMEA = "$GPGSV,3,1,11,03,45,162,45,07,55,052,48,08,30,295,42,09,22,120,43*7C";

// Messy NMEA data for decimal conversion testing
static const char *LAT = "4807.038";
static const char *LON = "01131.000";

// ========== TESTS ==========

ZTEST(gps_suite, test_valid_gprmc_sentence)
{
    zassert_true(nmea_parser(VALID_GPRMC), "GPRMC parsing failed with valid input");
}

ZTEST(gps_suite, test_valid_gpgga_sentence)
{
    zassert_true(nmea_parser(VALID_GPGGA), "GPGGA parsing failed with valid input");
}

ZTEST(gps_suite, test_invalid_checksum_sentence)
{
    zassert_false(nmea_parser(INVALID_CHECKSUM), "Expected parsing failure on bad checksum");
}

ZTEST(gps_suite, test_missing_checksum_symbol)
{
    zassert_false(nmea_parser(NO_CHECKSUM_MARKER), "Expected failure due to missing *");
}

ZTEST(gps_suite, test_unsupported_nmea_sentence)
{
    zassert_false(nmea_parser(UNSUPPORTED_NMEA), "Unsupported sentence type should not be parsed");
}

ZTEST(gps_suite, test_latitude_decimal_conversion)
{
    float result = nmea_to_decimal(LAT, "N", true);
    zassert_within((double)result, 48.1173, 0.0001, "Latitude conversion inaccurate");
}

ZTEST(gps_suite, test_longitude_decimal_conversion)
{
    float result = nmea_to_decimal(LON, "E", false);
    zassert_within((double)result, 11.5167, 0.0001, "Longitude conversion inaccurate");
}

ZTEST(gps_suite, test_southern_western_negative_coords)
{
    float lat_s = nmea_to_decimal("4807.038", "S", true);
    float lon_w = nmea_to_decimal("01131.000", "W", false);
    zassert_true(lat_s < 0 && lon_w < 0, "Expected negative values for S/W hemisphere");
}

ZTEST_SUITE(gps_suite, NULL, NULL, NULL, NULL, NULL);
