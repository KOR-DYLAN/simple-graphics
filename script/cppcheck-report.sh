#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOPDIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

BUILD_DIR=${BUILD_DIR:-"${TOPDIR}/build"}
REPORT_ROOT=${REPORT_ROOT:-"${TOPDIR}/report"}
COMPILE_DB=${COMPILE_DB:-"${BUILD_DIR}/compile_commands.json"}
CPPCHECK_BIN=${CPPCHECK:-cppcheck}
CPPCHECK_HTMLREPORT_BIN=${CPPCHECK_HTMLREPORT:-cppcheck-htmlreport}
WITH_CPPCHECK_MISRA=${WITH_CPPCHECK_MISRA:-ON}
CPPCHECK_MAX_CTU_DEPTH=${CPPCHECK_MAX_CTU_DEPTH:-4}
CPPCHECK_JOBS=${CPPCHECK_JOBS:-1}

if ! command -v "${CPPCHECK_BIN}" >/dev/null 2>&1; then
    echo "error: cppcheck was not found: ${CPPCHECK_BIN}" >&2
    exit 1
fi

if [ ! -f "${COMPILE_DB}" ]; then
    echo "error: compile database was not found: ${COMPILE_DB}" >&2
    echo "hint: run 'make config' first, or set COMPILE_DB=/path/to/compile_commands.json" >&2
    exit 1
fi

REPORT_DATE=${REPORT_DATE:-$(date +%Y%m%d-%H%M%S)}
COMMIT_ID=${REPORT_COMMIT:-$(git -C "${TOPDIR}" rev-parse --short=12 HEAD 2>/dev/null || echo "no-git")}
REPORT_DIR=${REPORT_DIR:-"${REPORT_ROOT}/${REPORT_DATE}-${COMMIT_ID}"}
CPPCHECK_BUILD_DIR="${REPORT_DIR}/cppcheck-build"
MISRA_CONFIG="${REPORT_DIR}/cppcheck-misra.json"
XML_REPORT="${REPORT_DIR}/cppcheck.xml"
TEXT_REPORT="${REPORT_DIR}/cppcheck.txt"
HTML_REPORT_DIR="${REPORT_DIR}/html"
HTML_INDEX="${HTML_REPORT_DIR}/index.html"
CHECKERS_REPORT="${REPORT_DIR}/cppcheck-checkers.txt"
MISRA_SUMMARY_REPORT="${REPORT_DIR}/cppcheck-misra-summary.txt"
SUMMARY_REPORT="${REPORT_DIR}/summary.txt"
METADATA_REPORT="${REPORT_DIR}/metadata.txt"
STATUS_REPORT="${REPORT_DIR}/git-status.txt"
STAGED_STAT_REPORT="${REPORT_DIR}/git-staged-stat.txt"

mkdir -p "${REPORT_DIR}" "${CPPCHECK_BUILD_DIR}"

write_misra_config()
{
    MISRA_ARGS_JSON=""

    if [ -n "${CPPCHECK_MISRA_RULE_TEXTS:-}" ]; then
        if [ ! -f "${CPPCHECK_MISRA_RULE_TEXTS}" ]; then
            echo "error: CPPCHECK_MISRA_RULE_TEXTS does not exist: ${CPPCHECK_MISRA_RULE_TEXTS}" >&2
            exit 1
        fi
        MISRA_RULE_TEXTS_ESCAPED=$(printf '%s' "${CPPCHECK_MISRA_RULE_TEXTS}" | sed 's/\\/\\\\/g; s/"/\\"/g')
        MISRA_ARGS_JSON="\"--rule-texts=${MISRA_RULE_TEXTS_ESCAPED}\""
    fi

    {
        echo "{"
        echo "    \"script\": \"misra.py\","
        echo "    \"args\": [${MISRA_ARGS_JSON}],"
        echo "    \"ctu\": true"
        echo "}"
    } > "${MISRA_CONFIG}"
}

run_cppcheck()
{
    OUTPUT_MODE=$1
    OUTPUT_FILE=$2
    CHECKERS_OPTION=$3

    set -- \
        --project="${COMPILE_DB}" \
        --cppcheck-build-dir="${CPPCHECK_BUILD_DIR}" \
        --enable=all \
        --inconclusive \
        --check-level=exhaustive \
        --check-library \
        --force \
        --quiet \
        --inline-suppr \
        --suppress=missingIncludeSystem \
        --relative-paths="${TOPDIR}" \
        --std=c99 \
        --max-ctu-depth="${CPPCHECK_MAX_CTU_DEPTH}" \
        -j "${CPPCHECK_JOBS}" \
        --output-file="${OUTPUT_FILE}"

    if [ "${WITH_CPPCHECK_MISRA}" != "OFF" ]; then
        set -- "$@" --addon="${MISRA_CONFIG}"
    fi

    if [ "${CHECKERS_OPTION}" = "ON" ]; then
        set -- "$@" --checkers-report="${CHECKERS_REPORT}"
    fi

    case "${OUTPUT_MODE}" in
    xml)
        set -- "$@" --xml --xml-version=2
        ;;
    text)
        set -- "$@" --template="{file}:{line}:{column}: {severity}: {id}: {message}"
        ;;
    *)
        echo "error: unsupported cppcheck output mode: ${OUTPUT_MODE}" >&2
        exit 1
        ;;
    esac

    "${CPPCHECK_BIN}" "$@"
}

write_metadata()
{
    {
        echo "Cppcheck ASPICE-oriented static analysis report"
        echo "================================================"
        echo "Generated at      : ${REPORT_DATE}"
        echo "Repository        : ${TOPDIR}"
        echo "Commit            : ${COMMIT_ID}"
        echo "Compile database  : ${COMPILE_DB}"
        echo "Cppcheck          : $(${CPPCHECK_BIN} --version)"
        if command -v "${CPPCHECK_HTMLREPORT_BIN}" >/dev/null 2>&1; then
            echo "HTML report tool  : ${CPPCHECK_HTMLREPORT_BIN}"
        else
            echo "HTML report tool  : not found"
        fi
        echo "MISRA addon       : ${WITH_CPPCHECK_MISRA}"
        echo "MISRA summary     : ${MISRA_SUMMARY_REPORT}"
        echo "Max CTU depth     : ${CPPCHECK_MAX_CTU_DEPTH}"
        echo "Jobs              : ${CPPCHECK_JOBS}"
        echo
        echo "Scope"
        echo "-----"
        echo "This script enables all Cppcheck checker classes available from the"
        echo "installed tool: warning, style, performance, portability, information,"
        echo "unusedFunction and missingInclude. It also enables inconclusive findings,"
        echo "exhaustive checking, library checks and CTU analysis."
        echo
        echo "ASPICE note"
        echo "-----------"
        echo "Automotive SPICE 4.0 process compliance cannot be proven by Cppcheck."
        echo "This report captures the static-analysis evidence that Cppcheck can"
        echo "produce for code quality and defect detection review."
        echo
        echo "Cppcheck checkers-report note"
        echo "-----------------------------"
        echo "The Cppcheck checkers report lists built-in checker activation state."
        echo "Addon findings, including MISRA C:2012 findings, are reported in the"
        echo "XML and text output. Use the MISRA summary file for addon evidence."
    } > "${METADATA_REPORT}"

    git -C "${TOPDIR}" status --short > "${STATUS_REPORT}" 2>/dev/null || true
    git -C "${TOPDIR}" diff --cached --stat > "${STAGED_STAT_REPORT}" 2>/dev/null || true
}

annotate_checkers_report()
{
    TEMP_CHECKERS_REPORT="${CHECKERS_REPORT}.tmp"

    if [ -f "${CHECKERS_REPORT}" ]; then
        {
            echo "Cppcheck addon status"
            echo "---------------------"
            if [ "${WITH_CPPCHECK_MISRA}" != "OFF" ]; then
                echo "MISRA C:2012 addon was requested with --addon=${MISRA_CONFIG}"
                echo "Cppcheck checkers-report may still say 'Misra is not enabled'"
                echo "because that section describes built-in checkers, not addon output."
                echo "See cppcheck-misra-summary.txt, cppcheck.xml and cppcheck.txt."
            else
                echo "MISRA C:2012 addon was disabled by WITH_CPPCHECK_MISRA=OFF."
            fi
            echo
            cat "${CHECKERS_REPORT}"
        } > "${TEMP_CHECKERS_REPORT}"
        mv "${TEMP_CHECKERS_REPORT}" "${CHECKERS_REPORT}"
    fi
}

write_misra_summary()
{
    {
        echo "MISRA C:2012 addon summary"
        echo "=========================="
        echo "MISRA addon : ${WITH_CPPCHECK_MISRA}"
        echo
        if [ "${WITH_CPPCHECK_MISRA}" = "OFF" ]; then
            echo "MISRA addon was disabled."
        else
            echo "Addon config : ${MISRA_CONFIG}"
            echo
            echo "Findings by MISRA id"
            echo "--------------------"
            awk '
                /<error / {
                    id = $0
                    sub(/^.*id="/, "", id)
                    sub(/".*$/, "", id)
                    if ((id ~ /^misra-c2012-/) || (id == "misra-config")) {
                        count[id] += 1
                        total += 1
                    }
                }
                END {
                    if (total == 0) {
                        print "none: 0"
                    } else {
                        for (id in count) {
                            print id ": " count[id]
                        }
                        print "total: " total
                    }
                }
            ' "${XML_REPORT}"
        fi
    } > "${MISRA_SUMMARY_REPORT}"
}

write_html_report()
{
    if command -v "${CPPCHECK_HTMLREPORT_BIN}" >/dev/null 2>&1; then
        mkdir -p "${HTML_REPORT_DIR}"
        "${CPPCHECK_HTMLREPORT_BIN}" \
            --file="${XML_REPORT}" \
            --report-dir="${HTML_REPORT_DIR}" \
            --source-dir="${TOPDIR}" \
            --title="SGL cppcheck ${COMMIT_ID}" >/dev/null
    fi
}

write_summary()
{
    {
        echo "Summary"
        echo "======="
        echo "Report directory : ${REPORT_DIR}"
        echo "XML report       : ${XML_REPORT}"
        echo "Text report      : ${TEXT_REPORT}"
        if [ -f "${HTML_INDEX}" ]; then
            echo "HTML report      : ${HTML_INDEX}"
        else
            echo "HTML report      : not generated"
        fi
        echo "Checkers report  : ${CHECKERS_REPORT}"
        echo "MISRA summary    : ${MISRA_SUMMARY_REPORT}"
        echo "Metadata         : ${METADATA_REPORT}"
        echo
        echo "Findings by severity"
        echo "--------------------"
        awk '
            /<error / {
                severity = $0
                sub(/^.*severity="/, "", severity)
                sub(/".*$/, "", severity)
                count[severity] += 1
                total += 1
            }
            END {
                if (total == 0) {
                    print "none: 0"
                } else {
                    for (severity in count) {
                        print severity ": " count[severity]
                    }
                    print "total: " total
                }
            }
        ' "${XML_REPORT}"
    } > "${SUMMARY_REPORT}"
}

if [ "${WITH_CPPCHECK_MISRA}" != "OFF" ]; then
    write_misra_config
fi

write_metadata
run_cppcheck xml "${XML_REPORT}" ON
annotate_checkers_report
run_cppcheck text "${TEXT_REPORT}" OFF
write_misra_summary
write_html_report
write_summary

echo "cppcheck report: ${REPORT_DIR}"
if [ -f "${HTML_INDEX}" ]; then
    echo "cppcheck html  : ${HTML_INDEX}"
fi
