#!/usr/bin/env python3

import sys
import json
import os

def log_message(message):
    """Logs a message to a file in the user's home directory."""
    log_file = os.path.expanduser("~/post_job_log.txt")
    with open(log_file, "a") as f:
        f.write(f"{message}\n")

def main():
    """
    Parses JSON from stdin and logs relevant information from a post-job event.
    """
    try:
        # Read the JSON data from standard input
        input_data = sys.stdin.read()
        if not input_data:
            log_message("No data received from stdin.")
            return

        data = json.loads(input_data)

        script_type = data.get("script_type", "UNKNOWN")
        log_message(f"Script Type: {script_type}")

        if script_type == "POST_JOB":
            # Log job details
            job_info = data.get("job")
            if job_info:
                target_name = job_info.get("target_name", "Unknown Target")
                log_message(f"Job for target '{target_name}' completed.")

            # Log sequence progress
            progress_info = data.get("progress")
            if progress_info:
                completed = progress_info.get("completed", 0)
                total = progress_info.get("total", 0)
                log_message(f"Sequence Progress: {completed}/{total} jobs complete.")
                if completed == total:
                    log_message("All sequence jobs are complete!")

    except json.JSONDecodeError:
        log_message(f"Failed to decode JSON: {input_data}")
    except Exception as e:
        log_message(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    main()
