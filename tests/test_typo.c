/* Unit tests for src/typo.h: make test */
#include <locale.h>
#include <stdio.h>

#include "../src/typo.h"

static int failed;

static void
check_distance(const char *pattern, const char *text, int expected)
{
	int got = typo_distance(pattern, text, 9);

	if (got != expected) {
		printf("FAIL distance(\"%s\", \"%s\") = %d, expected %d\n",
		       pattern, text, got, expected);
		failed++;
	}
}

static void
check_max(size_t len, int expected)
{
	int got = typo_max_errors(len, TYPO_MIN_LEN_1, TYPO_MIN_LEN_2);

	if (got != expected) {
		printf("FAIL max_errors(%zu) = %d, expected %d\n", len, got, expected);
		failed++;
	}
}

int
main(void)
{
	if (!setlocale(LC_CTYPE, "C.UTF-8"))
		setlocale(LC_CTYPE, "en_US.UTF-8");

	/* exact and substring matches cost nothing */
	check_distance("firefox", "firefox", 0);
	check_distance("fox", "firefox", 0);
	check_distance("vscod", "code-vscode", 0);
	check_distance("", "anything", 0);
	/* one error of each kind */
	check_distance("frefox", "firefox", 1);   /* deleted */
	check_distance("fireefox", "firefox", 1); /* inserted */
	check_distance("firefix", "firefox", 1);  /* substituted */
	check_distance("firefxo", "firefox", 1);  /* swapped */
	check_distance("fierfox", "firefox", 1);
	/* several errors */
	check_distance("thndrbird", "thunderbird", 2);
	check_distance("xyzzy", "firefox", 4);
	/* case and UTF-8: one error per character, not per byte */
	check_distance("FireFox", "firefox", 0);
	check_distance("телеграм", "Telegram - Телеграмм", 0);
	check_distance("телграм", "Телеграм", 1);
	check_distance("тлеграм", "Телеграм", 1);
	check_distance("Привет", "привет мир", 0);
	/* diacritics are ignored, in any script */
	check_distance("елка", "Ёлка", 0);
	check_distance("ЁЛКА", "елка", 0);
	check_distance("ее", "её", 0);
	check_distance("cafe", "Café", 0);
	check_distance("uber", "Über", 0);
	check_distance("cafe", "Cafe\xcc\x81", 0); /* decomposed: e + U+0301 */
	check_distance("καλημερα", "Καλημέρα", 0);
	check_distance("zurich", "Zürich", 0);
	check_distance("zuerich", "Zürich", 1); /* ue is not u: one error */
	if (!typo_contains("ёлк", "новогодняя елка") || typo_contains("олк", "елка")) {
		printf("FAIL contains\n");
		failed++;
	}
	/* case sensitive mode: diacritics are still ignored */
	typo_ignore_case = 0;
	check_distance("Cafe", "Café", 0);
	check_distance("cafe", "Café", 1);
	check_distance("Елка", "Ёлка", 0);
	typo_ignore_case = 1;
	/* the result is capped at max + 1 */
	if (typo_distance("xyzzy", "firefox", 2) != 3) {
		printf("FAIL cap\n");
		failed++;
	}
	/* allowed errors by pattern length */
	check_max(4, 0);
	check_max(5, 1);
	check_max(7, 1);
	check_max(8, 2);
	/* UTF-8 length */
	if (typo_strlen("телеграм") != 8) {
		printf("FAIL strlen\n");
		failed++;
	}

	if (failed) {
		printf("%d test(s) failed\n", failed);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
