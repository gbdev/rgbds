printt "main charmap\n"

charmap "ab", $0

x = "ab"
printt "{x}\n"

printt "newcharmap map1\n"
newcharmap map1

x = "ab"
printt "{x}\n"

printt "newcharmap map2, main\n"
newcharmap map2, main

x = "ab"
printt "{x}\n"

printt "setcharmap map1\n"
setcharmap map1

x = "ab"
printt "{x}\n"

printt "newcharmap map3\n"
newcharmap map3

charmap "ab", $1

x = "ab"
printt "{x}\n"

printt "newcharmap map4, map3\n"
newcharmap map4, map3

charmap "ab", $1
charmap "cd", $2

x = "ab"
printt "{x}\n"

x = "cd"
printt "{x}\n"

printt "setcharmap map3\n"
setcharmap map3

x = "ab"
printt "{x}\n"

x = "cd"
printt "{x}\n"

printt "setcharmap main\n"
setcharmap main

SECTION "sec0", ROM0

x = "ab"
printt "{x}\n"

printt "override main charmap\n"
charmap "ef", $3

x = "ab"
printt "{x}\n"

x = "ef"
printt "{x}\n"

printt "setcharmap map3\n"
setcharmap map3

x = "ab"
printt "{x}\n"

x = "cd"
printt "{x}\n"

x = "ef"
printt "{x}\n"

printt "newcharmap map1\n"
newcharmap map1

printt "setcharmap map5\n"
setcharmap map5
