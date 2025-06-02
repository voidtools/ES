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
[See Also](#See-Also)<br/>
<br/><br/><br/>



Download
--------

https://www.voidtools.com/downloads#cli
<br/><br/><br/>



![image](https://github.com/user-attachments/assets/0fcbe74a-c24c-4065-8a14-85757d525212)
<br/><br/><br/>



Install Guide
-------------

To install ES:
*   [Download](#download) ES and extract ES.exe to:<br>
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
<dt>-r &lt;search&gt;, -regex &lt;search&gt;</dt>
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
<dd>Highlight color 0-255.</dd>
<br/>

   -csv
   -efu
   -txt
   -m3u
   -m3u8
   -tsv
        Change display format.

   -size-format <format>
        0=auto, 1=Bytes, 2=KB, 3=MB.
   -date-format <format>
        0=auto, 1=ISO-8601, 2=FILETIME, 3=ISO-8601(UTC)

   -filename-color <color>
   -name-color <color>
   -path-color <color>
   -extension-color <color>
   -size-color <color>
   -date-created-color <color>, -dc-color <color>
   -date-modified-color <color>, -dm-color <color>
   -date-accessed-color <color>, -da-color <color>
   -attributes-color <color>
   -file-list-filename-color <color>
   -run-count-color <color>
   -date-run-color <color>
   -date-recently-changed-color <color>, -rc-color <color>
        Set the column color 0-255.

   -filename-width <width>
   -name-width <width>
   -path-width <width>
   -extension-width <width>
   -size-width <width>
   -date-created-width <width>, -dc-width <width>
   -date-modified-width <width>, -dm-width <width>
   -date-accessed-width <width>, -da-width <width>
   -attributes-width <width>
   -file-list-filename-width <width>
   -run-count-width <width>
   -date-run-width <width>
   -date-recently-changed-width <width>, -rc-width <width>
        Set the column width 0-200.

   -no-digit-grouping
        Don't group numbers with commas.
   -size-leading-zero
   -run-count-leading-zero
        Format the number with leading zeros, use with -no-digit-grouping.
   -double-quote
        Wrap paths and filenames with double quotes.
</dl>
<br/><br/><br/>
