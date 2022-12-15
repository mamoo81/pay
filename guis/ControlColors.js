
function applySkin(item) {
    if (Pay.useDarkSkin)
        applyDarkSkin(item);
    else
        applyLightSkin(item)
}

function applyDarkSkin(item) {
    item.palette.alternateBase = "#292c30";
    item.palette.base = "#232629";
    item.palette.brightText = "#fcfcfc"
    item.palette.button = "#4d4d4d"
    item.palette.buttonText = "#fcfcfc"
    item.palette.dark = "#bdc3c7"
    item.palette.highlight = "#76bfc7"
    item.palette.highlightedText = "#090909"
    item.palette.light = "#202326"
    item.palette.link = "#2980b9"
    item.palette.linkVisited = "#7f8c8d"
    item.palette.mid = "#6b6b6b"
    item.palette.midlight = "#41464c"
    item.palette.shadow = "#111111"
    item.palette.text = "#b2b2b2"
    item.palette.toolTipBase = "#31363b"
    item.palette.toolTipText = "#eff0f1"
    item.palette.window = "#232629"
    item.palette.windowText = "#fcfcfc"
}
function applyLightSkin(item) {
    item.palette.alternateBase = "#d8dae0";
    item.palette.base = "#e8e7e6";
    item.palette.brightText = "#ffffff";
    item.palette.button = "#bfbebd";
    item.palette.buttonText = "#26282a";
    item.palette.dark = "#353637";
    item.palette.highlight = "#0066ff";
    item.palette.highlightedText = "#f9f9f9";
    item.palette.light = "#f6f6f6";
    item.palette.link = "#45a7d7";
    item.palette.linkVisited = "#7f8c8d";
    item.palette.mid = "#bdbdbd";
    item.palette.midlight = "#e4e4e4";
    item.palette.shadow = "#28282a";
    item.palette.text = "#353637";
    item.palette.toolTipBase = "#ffffff";
    item.palette.toolTipText = "#000000";
    item.palette.window = "#e0dfde";
    item.palette.windowText = "#26282a";
}
