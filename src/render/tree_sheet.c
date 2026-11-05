#include "render/tree_sheet.h"

#include <math.h>
#include <stdio.h>

#include "util/json_lite.h"

bool TreeSheetLoad(TreeSheet *sheet, const char *base, char *message, size_t size)
{
    *sheet = (TreeSheet){0};
    char json_path[256];
    char image_path[256];
    snprintf(json_path, sizeof(json_path), "%s.json", base);
    snprintf(image_path, sizeof(image_path), "%s.png", base);

    char *text = FileExists(json_path) ? LoadFileText(json_path) : NULL;
    if (text == NULL)
    {
        snprintf(message, size, "Could not read %s", json_path);
        return false;
    }

    double cell_width = 0;
    double cell_height = 0;
    double columns = 0;
    double rows = 0;
    double step = 0;
    double ppu = 0;
    double height = 0;
    bool valid = JsonReadNumber(text, "cell_width", &cell_width) &&
                 JsonReadNumber(text, "cell_height", &cell_height) &&
                 JsonReadNumber(text, "columns", &columns) && JsonReadNumber(text, "rows", &rows) &&
                 JsonReadNumber(text, "azimuth_step_deg", &step) &&
                 JsonReadNumber(text, "pixels_per_unit", &ppu) &&
                 JsonReadNumber(text, "tree_height_units", &height);
    int row_count = (int)rows;
    valid = valid && row_count >= 1 && row_count <= TREE_SHEET_MAX_ROWS &&
            JsonReadNumberArray(text, "look_up_deg", sheet->look_up_deg, TREE_SHEET_MAX_ROWS) ==
                row_count &&
            JsonReadNumberArray(text, "row_anchor_x", sheet->anchor_x, TREE_SHEET_MAX_ROWS) ==
                row_count &&
            JsonReadNumberArray(text, "row_anchor_y", sheet->anchor_y, TREE_SHEET_MAX_ROWS) ==
                row_count;
    UnloadFileText(text);
    if (!valid)
    {
        snprintf(message, size, "%s is not a valid tree file", json_path);
        return false;
    }

    sheet->texture = LoadTexture(image_path);
    if (!IsTextureValid(sheet->texture))
    {
        snprintf(message, size, "Could not load the image %s", image_path);
        return false;
    }
    SetTextureFilter(sheet->texture, TEXTURE_FILTER_POINT);
    sheet->cell_width = (int)cell_width;
    sheet->cell_height = (int)cell_height;
    sheet->columns = (int)columns;
    sheet->rows = row_count;
    sheet->azimuth_step_deg = (float)step;
    sheet->pixels_per_unit = (float)ppu;
    sheet->height_units = (float)height;
    return true;
}

void TreeSheetUnload(TreeSheet *sheet)
{
    if (IsTextureValid(sheet->texture))
    {
        UnloadTexture(sheet->texture);
    }
    *sheet = (TreeSheet){0};
}

int TreeSheetColumn(const TreeSheet *sheet, float azimuth_deg)
{
    int column = (int)lroundf(azimuth_deg / sheet->azimuth_step_deg);
    return ((column % sheet->columns) + sheet->columns) % sheet->columns;
}

int TreeSheetRow(const TreeSheet *sheet, float look_up_deg)
{
    int best = 0;
    for (int row = 1; row < sheet->rows; row++)
    {
        if (fabsf(sheet->look_up_deg[row] - look_up_deg) <
            fabsf(sheet->look_up_deg[best] - look_up_deg))
        {
            best = row;
        }
    }
    return best;
}
