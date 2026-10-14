static_assert !def(__SCOPE__)

section "test", rom0

static_assert !def(.)
static_assert !def(..)
static_assert #__SCOPE__ === ""

Alpha.local1:
static_assert !def(.)
static_assert #.. === "Alpha.local1"
static_assert #__SCOPE__ === ".."

Beta:
static_assert #. === "Beta"
static_assert !def(..)
static_assert #__SCOPE__ === "."

Alpha.local2:
static_assert #. === "Beta"
static_assert #.. === "Alpha.local2"
static_assert #__SCOPE__ === ".."

.newLocal:
static_assert #. === "Beta"
static_assert #.. === "Beta.newLocal"
static_assert #__SCOPE__ === ".."
