/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

function applySkin(item) {
    if (Pay.useDarkSkin)
        applyDarkSkin(item);
    else
        applyLightSkin(item)
}

function applyDarkSkin(item) {
    item.palette.window = "#222222" // background of all things.
    item.palette.base = "#111111"; // background of text fields, popup-menus
    item.palette.alternateBase = "#1d1d1d"; // background of even rows in lists

    item.palette.light = "#181818" // background of groupbox, odd rows, pages etc
    item.palette.dark = "#bdc3c7" // contrast to the previous one
    item.palette.mid = "#4d4d4d" // borders for groupbox, sliders, text-area

    item.palette.windowText = "#fcfcfc" // Most text (labels etc)
    item.palette.text = "#fcfcfc" // multiline textarea text color.
    item.palette.brightText = "#9f9f9f" // popup menu text, less strong text

    item.palette.button = "#4d4d4d" // buttons and groupbox borders and slider borders
    item.palette.buttonText = "#fcfcfc" // just text for buttons

    item.palette.highlight = "#76bfc7" // mouseover on popup, slider thumb, outline of active textfield, selected-text background
    item.palette.highlightedText = "#090909"// selected text

    // Desktop is the only one that used tooltips
    item.palette.toolTipBase = "#629c7b" // tooltip background
    item.palette.toolTipText = "#000000" // tooltip text and outline
    item.palette.shadow = "#90e4b5" // tooltip shadow

    // shade of Flowee-green.
    item.palette.midlight = "#4f7d63"

    // Not currently used
    item.palette.link = "#2980b9" // unused
    item.palette.linkVisited = "#7f8c8d" // unused
}
function applyLightSkin(item) {
    item.palette.window = "#e0dfde";
    item.palette.base = "#e8e7e6";
    item.palette.alternateBase = "#e2e1e0";
    item.palette.light = "#dddcdb";
    item.palette.dark = "#353637";
    item.palette.mid = "#bdbdbd";
    item.palette.windowText = "#26282a";
    item.palette.text = "#26282a";
    item.palette.brightText = "#555658";
    item.palette.button = "#bfbebd";
    item.palette.buttonText = "#26282a";
    item.palette.highlight = "#0066ff";
    item.palette.highlightedText = "#f9f9f9";
    item.palette.toolTipBase = "#ffffff";
    item.palette.toolTipText = "#000000";
    item.palette.shadow = "#28282a";
    item.palette.midlight = "#5c9e67"

    item.palette.link = "#45a7d7";
    item.palette.linkVisited = "#7f8c8d";
}
