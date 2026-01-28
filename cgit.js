/* cgit.js: javascript functions for cgit
 *
 * Copyright (C) 2006-2018 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

/* Theme management */
var THEME_KEY = 'cgit-theme';
var THEMES = ['auto', 'light', 'dark'];

function getStoredTheme() {
	try {
		return localStorage.getItem(THEME_KEY);
	} catch (e) {
		return null;
	}
}

function getCurrentTheme() {
	return getStoredTheme() || 'auto';
}

function setTheme(theme) {
	try {
		if (theme === 'auto') {
			document.documentElement.removeAttribute('data-theme');
			localStorage.removeItem(THEME_KEY);
		} else {
			document.documentElement.setAttribute('data-theme', theme);
			localStorage.setItem(THEME_KEY, theme);
		}
	} catch (e) {
		if (theme !== 'auto') {
			document.documentElement.setAttribute('data-theme', theme);
		}
	}
	updateThemeToggle();
}

function cycleTheme() {
	var current = getCurrentTheme();
	var idx = THEMES.indexOf(current);
	var next = THEMES[(idx + 1) % THEMES.length];
	setTheme(next);
}

function updateThemeToggle() {
	var toggle = document.getElementById('theme-toggle');
	if (toggle) {
		toggle.textContent = getCurrentTheme();
	}
}

function initTheme() {
	var stored = getStoredTheme();
	if (stored) {
		document.documentElement.setAttribute('data-theme', stored);
	}
}

// Initialize theme immediately
initTheme();

(function () {

/* Age rendering - follows the logic and suffixes used in ui-shared.c */

var age_classes = [ "age-mins", "age-hours", "age-days",    "age-weeks",    "age-months",    "age-years" ];
var age_suffix =  [ "min.",     "hours",     "days",        "weeks",        "months",        "years",         "years" ];
var age_next =    [ 60,         3600,        24 * 3600,     7 * 24 * 3600,  30 * 24 * 3600,  365 * 24 * 3600, 365 * 24 * 3600 ];
var age_limit =   [ 7200,       24 * 7200,   7 * 24 * 7200, 30 * 24 * 7200, 365 * 25 * 7200, 365 * 25 * 7200 ];
var update_next = [ 10,         5 * 60,      1800,          24 * 3600,      24 * 3600,       24 * 3600,       24 * 3600 ];

function render_age(e, age) {
	var t, n;

	for (n = 0; n < age_classes.length; n++)
		if (age < age_limit[n])
			break;

	t = Math.round(age / age_next[n]) + " " + age_suffix[n];

	if (e.textContent != t) {
		e.textContent = t;
		if (n == age_classes.length)
			n--;
		if (e.className != age_classes[n])
			e.className = age_classes[n];
	}
}

function aging() {
	var n, next = 24 * 3600,
	    now_ut = Math.round((new Date().getTime() / 1000));

	for (n = 0; n < age_classes.length; n++) {
		var m, elems = document.getElementsByClassName(age_classes[n]);

		if (elems.length && update_next[n] < next)
			next = update_next[n];

		for (m = 0; m < elems.length; m++) {
			var age = now_ut - elems[m].getAttribute("data-ut");

			render_age(elems[m], age);
		}
	}

	window.setTimeout(aging, next * 1000);
}

document.addEventListener("DOMContentLoaded", function() {
	updateThemeToggle();
	aging();
}, false);

})();
