# ES
The Command Line Interface for Everything.

[Download](#download)<br/>
[Install Guide](#Install-Guide)<br/>
[Usage](#Usage)<br/>
[Search Options](#Search-Options)<br/>
[Sort Options](#Sort-Options)<br/>
[Display Options](#Display-Options)<br/>
[Export Options](#Export-Options)<br/>
[General Options](#General-Options)<br/>
[Search Syntax](#Search-Syntax)<br/>
[Notes](#Notes)<br/>
[Error Levels](#Error-Levels)<br/>
[See Also](#See-Also)<br/>
<br/><br/><br/>



Download
--------

https://github.com/voidtools/ES/releases

https://www.voidtools.com/downloads#cli
<br/><br/><br/>



![image](https://github.com/user-attachments/assets/0fcbe74a-c24c-4065-8a14-85757d525212)
<br/><br/><br/>



Install Guide
-------------

To install ES:
*   [Download](#download) ES and extract es.exe to:<br>
    <code>%LOCALAPPDATA%\Microsoft\WindowsApps</code>

This location is set in the PATH environment variable and allows es to be called from any directory in a command prompt or powershell.
<br/><br/><br/>



Usage
-----

<code>es.exe [options] search text</code>

For example:

<code>es everything ext:exe;ini</code>
<br/><br/><br/>



Search Options
--------------

<dl>
<dt>-r, -regex</dt>
<dd>Search using regular expressions.</dd>
<dt>-i, -case</dt>
<dd>Match case.</dd>
<dt>-w, -ww, -whole-word, -whole-words</dt>
<dd>Match whole words.</dd>
<dt>-p, -match-path</dt>
<dd>Match full path and file name.</dd>
<dt>-a, -diacritics</dt>
<dd>Match diacritical marks.</dd>
<br/>
<dt>-o &lt;offset&gt;, -offset &lt;offset&gt;</dt>
<dd>Show results starting from offset.</dd>
<dt>-n &lt;num&gt;, -max-results &lt;num&gt;</dt>
<dd>Limit the number of results shown to &lt;num&gt;.</dd>
<br/>
<dt>-path &lt;path&gt;</dt>
<dd>Search for subfolders and files in path.</dd>
<dt>-parent-path &lt;path&gt;</dt>
<dd>Search for subfolders and files in the parent of path.</dd>
<dt>-parent &lt;path&gt;</dt></dd>
<dd>Search for files with the specified parent path.</dd>
<br/>
<dt>/ad</dt>
<dd>Folders only.</dd>
<dt>/a-d</dt>
<dd>Files only.</dd>
<dt>/a[RHSDAVNTPLCOIE]</dt>
<dd>DIR style attributes search.<br/>
R = Read only.<br/>
H = Hidden.<br/>
S = System.<br/>
D = Directory.<br/>
A = Archive.<br/>
V = Device.<br/>
N = Normal.<br/>
T = Temporary.<br/>
P = Sparse file.<br/>
L = Reparse point.<br/>
C = Compressed.<br/>
O = Offline.<br/>
I = Not content indexed.<br/>
E = Encrypted.<br/>
- = Prefix a flag with - to exclude.</dd>
</dl>
<br/><br/><br/>



Sort Options
------------

<dl>
<dt>-s</dt>
<dd>sort by full path.</dd>
<dt>-sort &lt;name[-ascending|-descending]&gt;, -sort-&lt;name&gt;[-ascending|-descending]</dt>
<dd>Set sort<br/>
name=name|path|size|extension|date-created|date-modified|date-accessed|<br>
attributes|file-list-file-name|run-count|date-recently-changed|date-run</dd>
<dt>-sort-ascending, -sort-descending</dt>
<dd>Set sort order</dd>
<br/>
<dt>/on, /o-n, /os, /o-s, /oe, /o-e, /od, /o-d</dt>
<dd>DIR style sorts.<br/>
N = Name.<br/>
S = Size.<br/>
E = Extension.<br/>
D = Date modified.<br/>
- = Sort in descending order.</dd>
</dl>
<br/><br/><br/>



Display Options
---------------

<dl>
<dt>-name<br/>
-path-column<br/>
-full-path-and-name, -filename-column<br/>
-extension, -ext<br/>
-size<br/>
-date-created, -dc<br/>
-date-modified, -dm<br/>
-date-accessed, -da<br/>
-attributes, -attribs, -attrib<br/>
-file-list-file-name<br/>
-run-count<br/>
-date-run<br/>
-date-recently-changed, -rc</dt>
<dd>Show the specified column.</dd>
<br/>
<dt>-highlight</dt>
<dd>Highlight results.</dd>
<dt>-highlight-color &lt;color&gt;</dt>
<dd>Highlight color 0x00-0xFF:<br/>
<img src="https://github.com/user-attachments/assets/6f8b5573-152a-48fd-b340-8042aa54284f"></dd>
<br/>
<dt>-csv<br/>
-efu<br/>
-json<br/>
-m3u<br/>
-m3u8<br/>
-tsv<br/>
-txt</dt>
<dd>Change display format.</dd>
<br/>
<dt>-size-format &lt;format&gt;</dt>
<dd>0=auto, 1=Bytes, 2=KB, 3=MB.</dd>
<dt>-date-format &lt;format&gt;</dt>
<dd>0=auto, 1=ISO-8601, 2=FILETIME, 3=ISO-8601(UTC)</dd>
<br/>
<dt>-filename-color &lt;color&gt;<br/>
-name-color &lt;color&gt;<br/>
-path-color &lt;color&gt;<br/>
-extension-color &lt;color&gt;<br/>
-size-color &lt;color&gt;<br/>
-date-created-color &lt;color&gt;, -dc-color &lt;color&gt;<br/>
-date-modified-color &lt;color&gt;, -dm-color &lt;color&gt;<br/>
-date-accessed-color &lt;color&gt;, -da-color &lt;color&gt;<br/>
-attributes-color &lt;color&gt;<br/>
-file-list-filename-color &lt;color&gt;<br/>
-run-count-color &lt;color&gt;<br/>
-date-run-color &lt;color&gt;<br/>
-date-recently-changed-color &lt;color&gt;, -rc-color &lt;color&gt;</dt>
<dd>Set the column color 0x00-0xFF:<br/>
<img src="https://github.com/user-attachments/assets/6f8b5573-152a-48fd-b340-8042aa54284f"></dd>
<br/>
<dt>-filename-width &lt;width&gt;<br/>
-name-width &lt;width&gt;<br/>
-path-width &lt;width&gt;<br/>
-extension-width &lt;width&gt;<br/>
-size-width &lt;width&gt;<br/>
-date-created-width &lt;width&gt;, -dc-width &lt;width&gt;<br/>
-date-modified-width &lt;width&gt;, -dm-width &lt;width&gt;<br/>
-date-accessed-width &lt;width&gt;, -da-width &lt;width&gt;<br/>
-attributes-width &lt;width&gt;<br/>
-file-list-filename-width &lt;width&gt;<br/>
-run-count-width &lt;width&gt;<br/>
-date-run-width &lt;width&gt;<br/>
-date-recently-changed-width &lt;width&gt;, -rc-width &lt;width&gt;</dt>
<dd>Set the column width 0-200.</dd>
<br/>
<dt>-no-digit-grouping</dt>
<dd>Don't group numbers with commas.</dd>
<dt>-size-leading-zero<br/>
-run-count-leading-zero</dt>
<dd>Format the number with leading zeros, use with -no-digit-grouping.</dd>
<dt>-double-quote</dt>
<dd>Wrap paths and filenames with double quotes.</dd>
</dl>
<br/><br/><br/>



Export Options
--------------

<dl>
<dt>-export-csv &lt;out.csv&gt;<br/>
-export-efu &lt;out.efu&gt;<br/>
-export-json &lt;out.json&gt;<br/>
-export-m3u &lt;out.m3u&gt;<br/>
-export-m3u8 &lt;out.m3u8&gt;<br/>
-export-tsv &lt;out.txt&gt;<br/>
-export-txt &lt;out.txt&gt;</dt>
<dd>Export to a file using the specified layout.</dd>
<dt>-no-header</dt>
<dd>Do not output a column header for CSV, EFU and TSV files.</dd>
<dt>-utf8-bom</dt>
<dd>Store a UTF-8 byte order mark at the start of the exported file.</dd>
</dl>
<br/><br/><br/>



General Options
---------------

<dl>
<dt>-h, -help</dt>
<dd>Display this help.</dd>
<br/>
<dt>-instance &lt;name&gt;</dt>
<dd>Connect to the unique Everything instance name.</dd>
<dt>-ipc1, -ipc2</dt>
<dd>Use IPC version 1 or 2.</dd>
<dt>-pause, -more</dt>
<dd>Pause after each page of output.</dd>
<dt>-hide-empty-search-results</dt>
<dd>Don't show any results when there is no search.</dd>
<dt>-empty-search-help</dt>
<dd>Show help when no search is specified.</dd>
<dt>-timeout &lt;milliseconds&gt;</dt>
<dd>Timeout after the specified number of milliseconds to wait for<br/>
the Everything database to load before sending a query.</dd>
<br/>
<dt>-set-run-count &lt;filename&gt; &lt;count&gt;</dt>
<dd>Set the run count for the specified filename.</dd>
<dt>-inc-run-count &lt;filename&gt;</dt>
<dd>Increment the run count for the specified filename by one.</dd>
<dt>-get-run-count &lt;filename&gt;</dt>
<dd>Display the run count for the specified filename.</dd>
<dt>-get-result-count</dt>
<dd>Display the result count for the specified search.</dd>
<dt>-get-total-size</dt>
<dd>Display the total result size for the specified search.</dd>
<dt>-save-settings, -clear-settings</dt>
<dd>Save or clear settings.</dd>
<dt>-version</dt>
<dd>Display ES major.minor.revision.build version and exit.</dd>
<dt>-get-everything-version</dt>
<dd>Display Everything major.minor.revision.build version and exit.</dd>
<dt>-exit</dt>
<dd>Exit Everything.<br/>
Returns after Everything process closes.</dd>
<dt>-save-db</dt>
<dd>Save the Everything database to disk.<br/>
Returns after saving completes.</dd>
<dt>-reindex</dt>
<dd>Force Everything to reindex.<br/>
Returns after indexing completes.</dd>
<dt>-no-result-error</dt>
<dd>Set the error level if no results are found.</dd>
</dl>
<br/><br/><br/>



Search Syntax
-------------

<table>
<tr><td><i>space</i></td><td>AND</td></tr>
<tr><td><code>|</code></td><td>OR</td></tr>
<tr><td><code>!</td><td>NOT</td></tr>
<tr><td><code>< ></code></td><td>Grouping</td></tr>
<tr><td><code>" "</code></td><td>Escape operator characters.</td></tr>
</table>
<br/><br/><br/>



Notes
-----

Internal -'s in options can be omitted, eg: <code>-nodigitgrouping</code>

Switches can also start with a <code>/</code>

Use double quotes to escape spaces and switches.

Switches can be disabled by prefixing them with <code>no-</code>, eg: <code>-no-size</code>

Use a <code>^</code> prefix or wrap with double quotes (<code>"</code>) to escape <code>\ & | > < ^</code> in a command prompt.
<br/><br/><br/>



Error Levels
------------

Return codes from ES:
<table>
<tr><td>0</td><td>No known error, search successful.</td></tr>
<tr><td>1</td><td>Failed to register window class</td></tr>
<tr><td>2</td><td>Failed to create listening window.</td></tr>
<tr><td>3</td><td>Out of memory</td></tr>
<tr><td>4</td><td>Expected an additional command line option with the specified switch</td></tr>
<tr><td>5</td><td>Failed to create export output file</td></tr>
<tr><td>6</td><td>Unknown switch.</td></tr>
<tr><td>7</td><td>Failed to send Everything IPC a query.</td></tr>
<tr><td>8</td><td>No Everything IPC window - make sure the Everything search client is running.</td></tr>
<tr><td>9</td><td>No results found when used with <code>-no-result-error</code</td></tr>
</table>
<br/><br/><br/>



See Also
--------

*   https://www.voidtools.com/support/everything/command_line_interface/
*   https://www.voidtools.com/downloads#cli
*   https://www.voidtools.com/forum/viewtopic.php?t=5762
