#include "index_html.h"
#include <string.h>

const char part1[] = R""""(<html>
<body>
<form action="/results.html" target="_blank" method="post">
<label for="fname">Networks found:</label>
<br>
<select name="ssid">)"""";

const char part2[] = R""""(</select>
<br>
<label for="ipass">Security key:</label><br>
<input type="password" name="ipass"><br>
<input type="submit" value="Submit">
</form>
</body>
</html>)"""";

void set_index_html_content(char* buffer, const char* ssid_list, int ssid_count) {
	// Copy first part of the HTML page
	strcpy(buffer, part1);
	int i = 0;
	for (i = 0; i < ssid_count; i++) {
		strcat(buffer, "<option value=\"");
		strcat(buffer, ssid_list + i * 33);
		strcat(buffer, "\">");
		strcat(buffer, ssid_list + i * 33);
		strcat(buffer, "</option>");
	}
	// Copy second part of the HTML page
	strcat(buffer, part2);
}