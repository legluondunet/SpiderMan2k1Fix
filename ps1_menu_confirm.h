#ifndef SPIDERMAN2K1FIX_PS1_MENU_CONFIRM_H
#define SPIDERMAN2K1FIX_PS1_MENU_CONFIRM_H
/*
 * Pure C99 PS1/legacy menu confirmation policy. This helper never reads a
 * controller, synthesizes a key, modifies native game memory or logs.
 *
 * Confirm bit 0: Sony DS4 Cross/BUTTON1.
 * Confirm bit 1: Sony DS4 OPTIONS/BUTTON9.
 *
 * In PS1 preview, Cross is the ONLY synthetic confirmation, regardless of
 * the older ControllerEnterButtons preference. Outside that preview keep
 * the exact legacy Cross/Options/Both preference for compatibility.
 */
#define SPIDER_PS1_MENU_CROSS 1u
#define SPIDER_PS1_MENU_OPTIONS 2u

static unsigned spider_ps1_menu_confirm_mask(int ps1_preview,
                                              unsigned legacy_mask) {
  if(ps1_preview)return SPIDER_PS1_MENU_CROSS;
  return legacy_mask&(SPIDER_PS1_MENU_CROSS|SPIDER_PS1_MENU_OPTIONS);
}
static unsigned spider_ps1_menu_confirm_edges(unsigned current_buttons,
                                               unsigned previous_buttons,
                                               unsigned allowed_mask) {
  return (current_buttons&allowed_mask)&~previous_buttons;
}
static unsigned spider_ps1_menu_confirm_unowned(unsigned new_edges,
                                                unsigned previous_owner) {
  return new_edges&~previous_owner;
}
#endif
