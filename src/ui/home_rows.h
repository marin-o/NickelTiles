#ifndef NT_HOME_ROWS_H
#define NT_HOME_ROWS_H

// Kobo's own home rows (row1 to row3 in Nickel's mainContainer): finding them,
// and hiding or showing them for the grid. A row is only shown again if this
// mod hid it, since another mod (NickelHome) may have hidden some.

#include <QList>
#include <QWidget>

// The child of `parent` named `name`, or null.
QWidget *nt_find_direct_child(QWidget *parent, const char *name);

// The mainContainer that holds the three home rows, or null.
QWidget *nt_find_main_container(QWidget *homeView);

void nt_hide_row(QWidget *row);
void nt_show_row(QWidget *row);

// Keeps Kobo's top `keep` rows and hides the others. Returns the kept rows
// that are showing.
QList<QWidget *> nt_keep_rows(QWidget *container, int keep);

// The height a row really takes. Kobo's rows report a minimum height of 0 but
// take their size hint (about 490 px), so the hint is what they need.
int nt_row_height(const QWidget *row);

// The right half of Kobo's top row (row1col2), or null.
QWidget *nt_top_row_right(QWidget *container);

// Whether Kobo shows nothing in `half`: on a reader with no books it keeps the
// half visible, but with its current page hidden.
bool nt_half_is_empty(QWidget *half);

#endif
