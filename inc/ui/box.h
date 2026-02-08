#ifndef MEMSCOPE_BOX_H
#define MEMSCOPE_BOX_H

/**
 * Box Drawing Module
 * Provides functions for drawing bordered boxes with titles in ncurses.
 */

/**
 * Draw a box with Unicode box-drawing characters.
 * @param y Starting row
 * @param x Starting column
 * @param height Box height (including borders)
 * @param width Box width (including borders)
 * @param title Optional title (NULL for no title)
 */
void box_draw(int y, int x, int height, int width, const char *title);

/**
 * Draw a horizontal separator line within a box.
 * @param y Row to draw the separator
 * @param x Starting column
 * @param width Width of the separator
 */
void box_hline(int y, int x, int width);

#endif // MEMSCOPE_BOX_H
