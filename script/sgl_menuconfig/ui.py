# SPDX-License-Identifier: MIT

"""Curses widgets for build configuration and test selection."""

from __future__ import annotations

import curses
from pathlib import Path
import sys
from typing import Callable

from .kconfig import (
    dependency_is_met,
    normalize_option_value,
    resolve_option_values,
)
from .model import DialogLayout, OptionSpec, TestSpec


THEME = {
    "screen": 0,
    "dialog": 0,
    "title": curses.A_BOLD,
    "selected": curses.A_REVERSE | curses.A_BOLD,
    "button": curses.A_BOLD,
    "button_focus": curses.A_REVERSE | curses.A_BOLD,
    "shadow": curses.A_DIM,
    "footer": curses.A_BOLD,
    "disabled": curses.A_DIM,
}

def draw_line(
    window: curses.window,
    row: int,
    column: int,
    text: str,
    attribute: int = 0,
    limit: int | None = None,
) -> None:
    height, width = window.getmaxyx()

    if 0 <= row < height and column < width:
        available = max(0, width - column - 1)
        if limit is not None:
            available = min(available, max(0, limit))
        try:
            window.addnstr(row, column, text, available, attribute)
        except curses.error:
            pass


def draw_wrapped_field(
    window: curses.window,
    row: int,
    column: int,
    label: str,
    value: str,
    width: int,
    attribute: int,
) -> None:
    prefix = label + ": "
    first_width = max(1, width - len(prefix))
    first = value[:first_width]
    remaining = value[first_width:]
    draw_line(
        window, row, column, prefix + first, attribute, width
    )
    if remaining:
        second_prefix = " " * len(prefix)
        second_width = max(1, width - len(second_prefix))
        second = remaining[:second_width]
        if len(remaining) > second_width and second_width >= 3:
            second = second[:-3] + "..."
        draw_line(
            window, row + 1, column, second_prefix + second,
            attribute, width,
        )


def initialize_theme(window: curses.window) -> None:
    if curses.has_colors():
        try:
            curses.start_color()
            curses.use_default_colors()
            curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_MAGENTA)
            curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
            curses.init_pair(3, curses.COLOR_RED, curses.COLOR_WHITE)
            curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_RED)
            curses.init_pair(5, curses.COLOR_RED, curses.COLOR_WHITE)
            curses.init_pair(6, curses.COLOR_WHITE, curses.COLOR_RED)
            curses.init_pair(7, curses.COLOR_BLACK, curses.COLOR_BLACK)
            THEME.update({
                "screen": curses.color_pair(1),
                "dialog": curses.color_pair(2),
                "title": curses.color_pair(3) | curses.A_BOLD,
                "selected": curses.color_pair(4) | curses.A_BOLD,
                "button": curses.color_pair(5) | curses.A_BOLD,
                "button_focus": curses.color_pair(6) | curses.A_BOLD,
                "shadow": curses.color_pair(7),
                "footer": curses.color_pair(1) | curses.A_BOLD,
                "disabled": curses.color_pair(2) | curses.A_DIM,
            })
        except curses.error:
            pass

    window.bkgd(" ", THEME["screen"])


def make_dialog_layout(
    window: curses.window,
    max_height: int,
    max_width: int = 96,
) -> DialogLayout | None:
    screen_height, screen_width = window.getmaxyx()
    result = None

    if screen_height >= 20 and screen_width >= 72:
        height = min(max_height, screen_height - 5)
        width = min(max_width, screen_width - 8)
        top = max(1, (screen_height - height - 1) // 2)
        left = max(2, (screen_width - width) // 2)
        result = DialogLayout(top, left, height, width)

    return result


def draw_dialog(
    window: curses.window,
    layout: DialogLayout,
    title: str,
    section: str,
) -> None:
    shadow_width = min(
        layout.width, window.getmaxyx()[1] - layout.left - 3
    )
    for row in range(layout.top + 1, layout.bottom + 2):
        draw_line(
            window, row, layout.left + 2, " " * shadow_width,
            THEME["shadow"], shadow_width,
        )

    horizontal = "+" + ("-" * (layout.width - 2)) + "+"
    draw_line(
        window, layout.top, layout.left, horizontal,
        THEME["dialog"], layout.width,
    )
    for row in range(layout.top + 1, layout.bottom):
        draw_line(
            window, row, layout.left,
            "|" + (" " * (layout.width - 2)) + "|",
            THEME["dialog"], layout.width,
        )
    draw_line(
        window, layout.bottom, layout.left, horizontal,
        THEME["dialog"], layout.width,
    )

    title_text = " " + title + " "
    title_column = layout.left + max(1, (layout.width - len(title_text)) // 2)
    draw_line(
        window, layout.top, title_column, title_text,
        THEME["title"], len(title_text),
    )

    section_width = min(48, layout.width - 10)
    section_column = layout.left + (layout.width - section_width) // 2
    section_text = section.center(section_width)
    draw_line(
        window, layout.top + 2, section_column, section_text,
        THEME["selected"], section_width,
    )


def draw_footer(window: curses.window, text: str) -> None:
    height, width = window.getmaxyx()
    draw_line(
        window, height - 1, 1, text, THEME["footer"], width - 2
    )


def draw_button_bar(
    window: curses.window,
    layout: DialogLayout,
    row: int,
    labels: tuple[str, ...],
    selected: int,
    focused: bool,
) -> None:
    widths = tuple(len(label) + 4 for label in labels)
    total_width = sum(widths) + (2 * (len(labels) - 1))
    column = layout.left + max(2, (layout.width - total_width) // 2)

    for index, label in enumerate(labels):
        width = widths[index]
        attribute = (
            THEME["button_focus"]
            if focused and index == selected
            else THEME["button"]
        )
        draw_line(
            window, row, column, "+" + ("-" * (width - 2)) + "+",
            attribute, width,
        )
        draw_line(
            window, row + 1, column, "| " + label.center(width - 4) + " |",
            attribute, width,
        )
        draw_line(
            window, row + 2, column, "+" + ("-" * (width - 2)) + "+",
            attribute, width,
        )
        column += width + 2


def draw_small_terminal(window: curses.window) -> None:
    window.erase()
    height, width = window.getmaxyx()
    message = "Terminal is too small; resize to at least 72x20."
    column = max(0, (width - len(message)) // 2)
    draw_line(window, max(0, height // 2), column, message, THEME["footer"])
    draw_footer(window, "<Q>/<Esc> exits")
    window.refresh()


def show_help(
    window: curses.window,
    title: str,
    lines: tuple[str, ...],
) -> None:
    height = min(18, len(lines) + 7)
    layout = make_dialog_layout(window, height, 82)

    if layout is not None:
        draw_dialog(window, layout, title, "Help")
        row = layout.top + 4
        for line in lines:
            draw_line(
                window, row, layout.left + 3, line,
                THEME["dialog"], layout.width - 6,
            )
            row += 1
        draw_line(
            window, layout.bottom - 2, layout.left + 3,
            "Press Enter, Space, F1, Q, or Esc to return.",
            THEME["title"], layout.width - 6,
        )
        window.refresh()

        waiting = True
        while waiting:
            key = window.getch()
            if key in (
                ord("\n"), ord(" "), ord("q"), ord("Q"), 27,
                curses.KEY_ENTER, curses.KEY_F1,
            ):
                waiting = False


def edit_text(
    window: curses.window,
    label: str,
    initial: str,
    validator: Callable[[str], bool] | None = None,
) -> str | None:
    characters = list(initial)
    cursor = len(characters)
    message = ""
    result: str | None = None
    editing = True

    try:
        curses.curs_set(1)
    except curses.error:
        pass

    while editing:
        height, width = window.getmaxyx()
        prompt = label + ": "
        available = max(1, width - len(prompt) - 2)
        start = max(0, cursor - available + 1)
        visible = "".join(characters[start:start + available])

        window.move(max(0, height - 2), 0)
        window.clrtoeol()
        draw_line(window, height - 2, 0, prompt + visible, curses.A_REVERSE)
        window.move(max(0, height - 1), 0)
        window.clrtoeol()
        draw_line(
            window, height - 1, 0,
            message or "Enter: accept  Esc: cancel  Ctrl-U: clear",
        )
        try:
            window.move(
                max(0, height - 2),
                min(width - 2, len(prompt) + cursor - start),
            )
        except curses.error:
            pass
        window.refresh()
        key = window.get_wch()
        message = ""

        if key in ("\n", "\r") or key == curses.KEY_ENTER:
            candidate = "".join(characters)
            if validator is None or validator(candidate):
                result = candidate
                editing = False
            else:
                message = "Invalid value"
        elif key == "\x1b":
            editing = False
        elif key in ("\b", "\x7f") or key == curses.KEY_BACKSPACE:
            if cursor > 0:
                del characters[cursor - 1]
                cursor -= 1
        elif key == curses.KEY_DC:
            if cursor < len(characters):
                del characters[cursor]
        elif key == curses.KEY_LEFT:
            cursor = max(0, cursor - 1)
        elif key == curses.KEY_RIGHT:
            cursor = min(len(characters), cursor + 1)
        elif key == curses.KEY_HOME:
            cursor = 0
        elif key == curses.KEY_END:
            cursor = len(characters)
        elif key == "\x15":
            characters = []
            cursor = 0
        elif isinstance(key, str) and key.isprintable():
            characters.insert(cursor, key)
            cursor += 1

    try:
        curses.curs_set(0)
    except curses.error:
        pass

    return result


def is_valid_integer(value: str, minimum: int) -> bool:
    result = False

    try:
        result = int(value, 10) >= minimum
    except ValueError:
        result = False

    return result


class OptionMenu:
    BUTTONS = ("Defaults", "Save & Exit", "Exit", "Help")

    def __init__(
        self,
        window: curses.window,
        specs: tuple[OptionSpec, ...],
        values: dict[str, str],
        config_path: Path,
        title: str,
    ) -> None:
        self.window = window
        self.specs = specs
        self.values = values
        self.config_path = config_path
        self.title = title
        self.selected = 0
        self.offset = 0
        self.message = ""
        self.focus = "list"
        self.button = 0

    def format_marker(self, spec: OptionSpec) -> str:
        value = self.values[spec.name]
        result = "(*)" if value else "( )"

        if not dependency_is_met(spec, self.values):
            result = "---"
        elif spec.kind == "boolean":
            result = "[X]" if value == "ON" else "[ ]"
        elif spec.kind == "choice":
            result = "< " + value + " >"
        elif spec.kind == "integer":
            result = "(" + value + ")"

        return result

    def move_choice(self, spec: OptionSpec, direction: int) -> None:
        current = spec.choices.index(self.values[spec.name])
        next_index = (current + direction) % len(spec.choices)
        self.values[spec.name] = spec.choices[next_index]

    def resolve_values(self) -> None:
        resolved = resolve_option_values(self.specs, self.values)
        self.values.update(resolved)

    def activate(self, direction: int = 1) -> None:
        spec = self.specs[self.selected]

        if not dependency_is_met(spec, self.values):
            self.message = "Requires: " + spec.depends_on
        elif spec.kind == "boolean":
            old_value = self.values[spec.name]
            self.values[spec.name] = "OFF" if old_value == "ON" else "ON"
        elif spec.kind == "choice":
            self.move_choice(spec, direction)
        else:
            validator = None
            if spec.kind == "integer":
                validator = lambda value: is_valid_integer(
                    value, spec.minimum
                )
            value = edit_text(
                self.window, spec.label, self.values[spec.name], validator
            )
            if value is not None:
                self.values[spec.name] = value
        self.resolve_values()
        if (
            spec.kind == "boolean"
            and self.values[spec.name] == "ON"
            and (spec.selects or spec.assignments)
        ):
            self.message = "{} preset values applied".format(spec.label)

    def reset_selected(self) -> None:
        spec = self.specs[self.selected]
        if dependency_is_met(spec, self.values):
            self.values[spec.name] = spec.default
            self.resolve_values()
            self.message = "{} restored to its default".format(spec.label)
        else:
            self.message = "Requires: " + spec.depends_on

    def show_help(self) -> None:
        show_help(
            self.window,
            self.title,
            (
                "Up/Down or J/K       Move through build options.",
                "Space or Enter       Toggle or edit the selected option.",
                "Left/Right           Cycle a choice value.",
                "D                    Restore the selected default.",
                "Tab/Shift-Tab        Move between the list and buttons.",
                "F10 or S             Save configuration and exit.",
                "F1 or H              Open this help window.",
                "Q or Esc             Exit without saving.",
            ),
        )

    def activate_button(self) -> tuple[bool, bool]:
        saved = False
        running = True
        label = self.BUTTONS[self.button]

        if label == "Defaults":
            self.reset_selected()
            self.focus = "list"
        elif label == "Save & Exit":
            saved = True
            running = False
        elif label == "Exit":
            running = False
        else:
            self.show_help()

        return saved, running

    def move_focus(self, direction: int) -> None:
        if self.focus == "list":
            self.focus = "buttons"
            self.button = 0 if direction > 0 else len(self.BUTTONS) - 1
        else:
            next_button = self.button + direction
            if 0 <= next_button < len(self.BUTTONS):
                self.button = next_button
            else:
                self.focus = "list"

    def render(self) -> bool:
        self.window.erase()
        layout = make_dialog_layout(self.window, 28)
        result = layout is not None

        if layout is None:
            draw_small_terminal(self.window)
            return result

        draw_dialog(
            self.window, layout, self.title,
            "Build option selection",
        )
        body_top = layout.top + 4
        body_bottom = layout.bottom - 8
        body_height = max(1, body_bottom - body_top + 1)

        if self.selected < self.offset:
            self.offset = self.selected
        if self.selected >= self.offset + body_height:
            self.offset = self.selected - body_height + 1

        for row, index in enumerate(
                range(self.offset, min(len(self.specs),
                                       self.offset + body_height)),
                body_top):
            spec = self.specs[index]
            marker = self.format_marker(spec)
            text = "{:<18} {:<30} {}".format(
                marker, spec.label, spec.category
            )
            attribute = (
                THEME["selected"]
                if index == self.selected
                else (
                    THEME["dialog"]
                    if dependency_is_met(spec, self.values)
                    else THEME["disabled"]
                )
            )
            draw_line(
                self.window, row, layout.left + 3, text,
                attribute, layout.width - 6,
            )

        spec = self.specs[self.selected]
        position = "{}/{}".format(self.selected + 1, len(self.specs))
        draw_line(
            self.window, layout.top + 3, layout.right - len(position) - 2,
            position, THEME["title"], len(position),
        )
        draw_line(
            self.window, layout.bottom - 7, layout.left + 3,
            "{} = {}  [{}]".format(
                spec.name, self.values[spec.name] or "<empty>",
                spec.category,
            ),
            THEME["title"], layout.width - 6,
        )
        draw_line(
            self.window, layout.bottom - 6, layout.left + 3,
            self.message or (
                spec.description
                if dependency_is_met(spec, self.values)
                else "Requires: " + spec.depends_on
            ),
            THEME["dialog"], layout.width - 6,
        )
        draw_button_bar(
            self.window, layout, layout.bottom - 4, self.BUTTONS,
            self.button, self.focus == "buttons",
        )
        draw_footer(
            self.window,
            "<Tab>/<Shift-Tab> moves  <Space> selects  <F1> help  <F10> save",
        )
        self.window.refresh()
        return result

    def run(self) -> bool:
        self.window.keypad(True)
        try:
            curses.curs_set(0)
        except curses.error:
            pass
        saved = False
        running = True

        while running:
            layout_valid = self.render()
            key = self.window.getch()
            self.message = ""

            if key == curses.KEY_RESIZE:
                pass
            elif key in (ord("q"), ord("Q"), 27):
                running = False
            elif not layout_valid:
                pass
            elif key == ord("\t"):
                self.move_focus(1)
            elif key == curses.KEY_BTAB:
                self.move_focus(-1)
            elif key in (curses.KEY_F1, ord("h"), ord("H")):
                self.show_help()
            elif key in (ord("s"), ord("S"), curses.KEY_F10):
                saved = True
                running = False
            elif self.focus == "list":
                if key in (curses.KEY_UP, ord("k")):
                    self.selected = (
                        self.selected - 1
                    ) % len(self.specs)
                elif key in (curses.KEY_DOWN, ord("j")):
                    self.selected = (
                        self.selected + 1
                    ) % len(self.specs)
                elif key == curses.KEY_LEFT:
                    self.activate(-1)
                elif key == curses.KEY_RIGHT:
                    self.activate(1)
                elif key in (
                    ord(" "), ord("\n"), curses.KEY_ENTER, ord("e")
                ):
                    self.activate()
                elif key in (ord("d"), ord("D")):
                    self.reset_selected()
            elif key in (curses.KEY_LEFT, curses.KEY_UP):
                self.button = (
                    self.button - 1
                ) % len(self.BUTTONS)
            elif key in (curses.KEY_RIGHT, curses.KEY_DOWN):
                self.button = (
                    self.button + 1
                ) % len(self.BUTTONS)
            elif key in (ord(" "), ord("\n"), curses.KEY_ENTER):
                button_saved, running = self.activate_button()
                saved = saved or button_saved

        return saved


class RunMenu:
    BUTTONS = ("Edit Args", "Run", "Exit", "Help")

    def __init__(
        self,
        window: curses.window,
        tests: tuple[TestSpec, ...],
        selected: int,
        arguments: dict[str, str],
        config_path: Path,
        root: Path,
    ) -> None:
        self.window = window
        self.tests = tests
        self.selected = selected
        self.arguments = arguments
        self.config_path = config_path
        self.root = root
        self.offset = 0
        self.message = ""
        self.focus = "list"
        self.button = 1

    def show_help(self) -> None:
        show_help(
            self.window,
            "SGL Test Runner",
            (
                "Up/Down or J/K       Select a test program.",
                "E                    Edit arguments for that program.",
                "D                    Restore its built-in arguments.",
                "Tab/Shift-Tab        Move between the list and buttons.",
                "Enter, R, or F10     Save the selection and run it.",
                "F1 or H              Open this help window.",
                "Q or Esc             Exit without running.",
            ),
        )

    def move_focus(self, direction: int) -> None:
        if self.focus == "list":
            self.focus = "buttons"
            self.button = 0 if direction > 0 else len(self.BUTTONS) - 1
        else:
            next_button = self.button + direction
            if 0 <= next_button < len(self.BUTTONS):
                self.button = next_button
            else:
                self.focus = "list"

    def activate_button(self) -> tuple[bool, bool]:
        accepted = False
        running = True
        label = self.BUTTONS[self.button]

        if label == "Edit Args":
            self.edit_arguments()
            self.focus = "list"
        elif label == "Run":
            accepted = True
            running = False
        elif label == "Exit":
            running = False
        else:
            self.show_help()

        return accepted, running

    def display_arguments(self, value: str) -> str:
        result = value or "<none>"
        root = str(self.root)

        if root:
            result = result.replace(root, "${TOPDIR}")

        return result

    def render(self) -> bool:
        self.window.erase()
        layout = make_dialog_layout(self.window, 26)
        result = layout is not None

        if layout is None:
            draw_small_terminal(self.window)
            return result

        draw_dialog(
            self.window, layout, "SGL Test Runner",
            "Test program selection",
        )
        body_top = layout.top + 4
        body_bottom = layout.bottom - 10
        body_height = max(1, body_bottom - body_top + 1)

        if self.selected < self.offset:
            self.offset = self.selected
        if self.selected >= self.offset + body_height:
            self.offset = self.selected - body_height + 1

        for row, index in enumerate(
                range(self.offset, min(len(self.tests),
                                       self.offset + body_height)),
                body_top):
            test = self.tests[index]
            marker = "(*)" if index == self.selected else "( )"
            text = "{} {:<12} {}".format(
                marker, test.name, test.description
            )
            attribute = (
                THEME["selected"]
                if index == self.selected
                else THEME["dialog"]
            )
            draw_line(
                self.window, row, layout.left + 3, text,
                attribute, layout.width - 6,
            )

        test = self.tests[self.selected]
        default = self.display_arguments(test.default_args)
        current = self.display_arguments(self.arguments[test.name])
        draw_line(
            self.window, layout.top + 3, layout.left + 3,
            self.message or "Selected: " + test.name,
            THEME["title"], layout.width - 6,
        )
        draw_wrapped_field(
            self.window, layout.bottom - 9, layout.left + 3,
            "Default arguments", default, layout.width - 6,
            THEME["title"],
        )
        draw_wrapped_field(
            self.window, layout.bottom - 7, layout.left + 3,
            "Arguments", current, layout.width - 6,
            THEME["dialog"],
        )
        draw_button_bar(
            self.window, layout, layout.bottom - 4, self.BUTTONS,
            self.button, self.focus == "buttons",
        )
        draw_footer(
            self.window,
            "<Tab>/<Shift-Tab> moves  <E> edits args  <F1> help  <F10> run",
        )
        self.window.refresh()
        return result

    def edit_arguments(self) -> None:
        test = self.tests[self.selected]
        value = edit_text(
            self.window, test.name + " arguments",
            self.arguments[test.name],
        )
        if value is not None:
            self.arguments[test.name] = value

    def run(self) -> bool:
        self.window.keypad(True)
        try:
            curses.curs_set(0)
        except curses.error:
            pass
        accepted = False
        running = True

        while running:
            layout_valid = self.render()
            key = self.window.getch()
            self.message = ""

            if key == curses.KEY_RESIZE:
                pass
            elif key in (ord("q"), ord("Q"), 27):
                running = False
            elif not layout_valid:
                pass
            elif key == ord("\t"):
                self.move_focus(1)
            elif key == curses.KEY_BTAB:
                self.move_focus(-1)
            elif key in (curses.KEY_F1, ord("h"), ord("H")):
                self.show_help()
            elif key in (
                ord("\n"), curses.KEY_ENTER, ord("r"), ord("R"),
                curses.KEY_F10,
            ) and self.focus == "list":
                accepted = True
                running = False
            elif self.focus == "list":
                if key in (curses.KEY_UP, ord("k")):
                    self.selected = (
                        self.selected - 1
                    ) % len(self.tests)
                elif key in (curses.KEY_DOWN, ord("j")):
                    self.selected = (
                        self.selected + 1
                    ) % len(self.tests)
                elif key in (ord("e"), ord("E")):
                    self.edit_arguments()
                elif key in (ord("d"), ord("D")):
                    test = self.tests[self.selected]
                    self.arguments[test.name] = test.default_args
                    self.message = "Default arguments restored"
            elif key in (curses.KEY_LEFT, curses.KEY_UP):
                self.button = (
                    self.button - 1
                ) % len(self.BUTTONS)
            elif key in (curses.KEY_RIGHT, curses.KEY_DOWN):
                self.button = (
                    self.button + 1
                ) % len(self.BUTTONS)
            elif key in (ord(" "), ord("\n"), curses.KEY_ENTER):
                button_accepted, running = self.activate_button()
                accepted = accepted or button_accepted

        return accepted


def run_curses_menu(callback: Callable[[curses.window], bool]) -> bool:
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        raise RuntimeError("an interactive terminal is required")

    def run(window: curses.window) -> bool:
        initialize_theme(window)
        return callback(window)

    return curses.wrapper(run)

