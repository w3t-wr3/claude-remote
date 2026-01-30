#!/bin/bash
# update_manual.sh — Regenerate Claude Remote manual files for Flipper Zero SD card
#
# Usage:
#   ./update_manual.sh [output_dir]
#
# Default output: ./manual_output/
# Copy the contents to: /ext/apps_data/claude_remote/manual/ on the Flipper SD card
#
# This script generates .txt files formatted for the Flipper's 128x64 display
# (lines ≤30 chars for readability on FontSecondary).

set -euo pipefail

OUTPUT_DIR="${1:-./manual_output}"
mkdir -p "$OUTPUT_DIR"

echo "Generating Claude Remote manual files..."

cat > "$OUTPUT_DIR/01_getting_started.txt" << 'CHAPTER'
Claude Code is an agentic AI
coding tool by Anthropic.

It runs in your terminal and
can read, write, and execute
code across your project.

Start with: claude
New session: claude --new
Resume: claude --continue
Print: claude -p "query"

Key concepts:
- Projects: code in your cwd
- Sessions: conversation state
- Tools: file/bash/search ops
- Skills: loaded from CLAUDE.md
CHAPTER

cat > "$OUTPUT_DIR/02_keybindings.txt" << 'CHAPTER'
Navigation:
 Ctrl+C  Cancel/interrupt
 Ctrl+D  Exit Claude Code
 Up/Down Scroll history
 Esc     Cancel current input
 Tab     Accept autocomplete

Approval prompts:
 1       Yes / approve
 2       No / decline
 3       Other / alternative

Voice (macOS):
 F5 or fn+fn  Start dictation
 Configure in System Settings
  > Keyboard > Dictation

Terminal shortcuts:
 Ctrl+L  Clear screen
 Ctrl+A  Start of line
 Ctrl+E  End of line
 Ctrl+K  Delete to end
CHAPTER

cat > "$OUTPUT_DIR/03_slash_commands.txt" << 'CHAPTER'
Common slash commands:

 /help     Show help
 /clear    Clear conversation
 /compact  Summarize context
 /config   Open settings
 /cost     Show token usage
 /doctor   Check setup health
 /init     Create CLAUDE.md
 /review   Review PR changes
 /terminal-setup  Fix terminal

Session management:
 /compact  Reduce context size
 /clear    Start fresh

Project setup:
 /init     Create project file
 /config   Edit preferences

Debugging:
 /doctor   Diagnose issues
 /terminal-setup  Fix display
CHAPTER

cat > "$OUTPUT_DIR/04_skills.txt" << 'CHAPTER'
Skills are specialized
capabilities Claude loads
from CLAUDE.md or .claude/
files in your project.

Tools available to Claude:
 Read, Write, Edit files
 Bash (run commands)
 Grep, Glob (search)
 Task (sub-agents)
 WebFetch, WebSearch
 NotebookEdit (Jupyter)

Invoke skills:
 /skill command or just ask

Custom skills:
 Add to CLAUDE.md or
 .claude/skills/*.md

Agent types:
 Bash - command execution
 Explore - codebase search
 Plan - design strategy
 general-purpose - flexible
CHAPTER

cat > "$OUTPUT_DIR/05_playbooks.txt" << 'CHAPTER'
Recipe: New Project Setup
 1. claude --new
 2. /init to create CLAUDE.md
 3. Describe your project
 4. Let Claude scaffold

Recipe: Debug Failing Tests
 1. Paste test output
 2. Ask Claude to investigate
 3. Approve fixes with 1
 4. Run tests again

Recipe: Refactor Module
 1. Ask to read the module
 2. Describe desired changes
 3. Review diff, approve/deny
 4. Run tests to verify

Recipe: Code Review
 1. /review or paste PR URL
 2. Claude reads all changes
 3. Get feedback + suggestions

Recipe: Large Changes
 1. Use /init for CLAUDE.md
 2. Describe architecture
 3. Let Claude plan first
 4. Approve step by step
CHAPTER

echo "Generated files in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR"
echo ""
echo "Copy to Flipper SD card:"
echo "  cp $OUTPUT_DIR/*.txt /Volumes/FLIPPER/apps_data/claude_remote/manual/"
echo ""
echo "Or create the directory first:"
echo "  mkdir -p /Volumes/FLIPPER/apps_data/claude_remote/manual/"
