#ifndef BATTERY_MONITOR_UI_GRAPH_CLAMP_H__
#define BATTERY_MONITOR_UI_GRAPH_CLAMP_H__

/**
 * @brief Clamp a battery percentage read from the DB to 0..100.
 *
 * battery_to_pixel() returns -1 for out-of-range results, and the graph
 * code turns that into row (480 - (-1)) = 481, one row past the screen
 * surface. Clamping both ends keeps every plotted row inside the graph.
 */
static inline int graph_clampPercent(int perc)
{
    if (perc < 0)
        return 0;
    if (perc > 100)
        return 100;
    return perc;
}

#endif // BATTERY_MONITOR_UI_GRAPH_CLAMP_H__
