package com.dmitriy.u200btextmatrix;

public final class TextProcessor {
    public static final char ZWSP = '\u200B';

    public static final class Result {
        public final String text;
        public final int count;
        Result(String text, int count) { this.text = text; this.count = count; }
    }

    private TextProcessor() {}

    public static Result insert(String input) {
        input = remove(input == null ? "" : input);
        StringBuilder out = new StringBuilder(input.length() * 2);
        int square = 0, round = 0, added = 0;
        for (int i = 0; i < input.length();) {
            int cp = input.codePointAt(i);
            int n = Character.charCount(cp);
            if (cp == '[') square++;
            if (cp == '(') round++;
            boolean inTag = square > 0 || round > 0;
            out.appendCodePoint(cp);
            i += n;
            if (cp == ']') { square = Math.max(0, square - 1); continue; }
            if (cp == ')') { round = Math.max(0, round - 1); continue; }
            if (inTag || Character.isWhitespace(cp)) continue;
            while (i < input.length()) {
                int next = input.codePointAt(i);
                int type = Character.getType(next);
                if (type == Character.NON_SPACING_MARK || type == Character.COMBINING_SPACING_MARK || type == Character.ENCLOSING_MARK) {
                    out.appendCodePoint(next);
                    i += Character.charCount(next);
                } else break;
            }
            out.append(ZWSP);
            added++;
        }
        return new Result(out.toString(), added);
    }

    public static String remove(String s) {
        return s == null ? "" : s.replace(String.valueOf(ZWSP), "");
    }

    public static String debug(String s) {
        return s == null ? "" : s.replace(ZWSP, '*');
    }
}
