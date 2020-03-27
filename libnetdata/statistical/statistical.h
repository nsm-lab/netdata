// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETDATA_STATISTICAL_H
#define NETDATA_STATISTICAL_H 1

#include "../libnetdata.h"

extern void log_series_to_stderr(LONG_DOUBLE *series, size_t entries, calculated_number result, const char *msg);

extern LONG_DOUBLE average(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE moving_average(const LONG_DOUBLE *series, size_t entries, size_t period);
extern LONG_DOUBLE median(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE moving_median(const LONG_DOUBLE *series, size_t entries, size_t period);
extern LONG_DOUBLE running_median_estimate(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE standard_deviation(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE single_exponential_smoothing(const LONG_DOUBLE *series, size_t entries, LONG_DOUBLE alpha);
extern LONG_DOUBLE single_exponential_smoothing_reverse(const LONG_DOUBLE *series, size_t entries, LONG_DOUBLE alpha);
extern LONG_DOUBLE double_exponential_smoothing(const LONG_DOUBLE *series, size_t entries, LONG_DOUBLE alpha, LONG_DOUBLE beta, LONG_DOUBLE *forecast);
extern LONG_DOUBLE holtwinters(const LONG_DOUBLE *series, size_t entries, LONG_DOUBLE alpha, LONG_DOUBLE beta, LONG_DOUBLE gamma, LONG_DOUBLE *forecast);
extern LONG_DOUBLE sum_and_count(const LONG_DOUBLE *series, size_t entries, size_t *count);
extern LONG_DOUBLE sum(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE median_on_sorted_series(const LONG_DOUBLE *series, size_t entries);
extern LONG_DOUBLE *copy_series(const LONG_DOUBLE *series, size_t entries);
extern void sort_series(LONG_DOUBLE *series, size_t entries);

#endif //NETDATA_STATISTICAL_H
