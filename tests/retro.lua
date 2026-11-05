-- Isolated testbed supplement only. Does not load any desktop config/autostarts.
hl.config({
    general = { border_size = 1, gaps_in = 8, gaps_out = 20,
        ["col.active_border"] = "rgb(000000)",
        ["col.inactive_border"] = "rgb(000000)" },
    decoration = { rounding = 0, blur = { enabled = false }, shadow = { enabled = false } },
    animations = { enabled = false },
    input = { follow_mouse = 0 },
})
hl.window_rule({ match = { class = "^wmfloat.*$" }, float = true })
