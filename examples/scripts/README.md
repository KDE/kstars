# Ekos Capture Script Examples

This directory contains example Python scripts that can be used with the Ekos Capture module's pre/post-capture and pre/post-job script hooks. These scripts demonstrate how to receive and parse the JSON data sent from Ekos via standard input.

## How It Works

When a script hook is triggered in Ekos, it executes the specified script. The script receives a JSON payload on its standard input containing contextual information about the event.

The Python scripts in this directory (`post_capture_script.py` and `post_job_script.py`) are set up to:

1. Read the JSON data from `stdin`.
2. Parse the JSON into a Python dictionary.
3. Extract relevant information.
4. Log the information to a file in the user's home directory (`~/post_capture_log.txt` or `~/post_job_log.txt`).

## JSON Payload Structure

The JSON payload structure varies depending on the script type.

### All Script Types

All payloads include a `script_type` field and, if a job is active, a `job` object.

- `script_type`: (string) The type of event that triggered the script. Can be one of: `"PRE_CAPTURE"`, `"POST_CAPTURE"`, `"PRE_JOB"`, `"POST_JOB"`.
- `job`: (object) Contains details about the current sequence job.
  - `filter`: (string) The name of the filter being used.
  - `exposure`: (number) The exposure time in seconds.
  - `type`: (string) The frame type (e.g., `"Light"`, `"Dark"`, `"Flat"`).
  - `binning_x`: (integer) The X-axis binning value.
  - `binning_y`: (integer) The Y-axis binning value.
  - `target_name`: (string) The name of the astronomical object being captured.

### POST_CAPTURE Payload

In addition to the common fields, the `POST_CAPTURE` payload includes `fits` and `progress` objects.

- `fits`: (object) Contains information about the captured FITS file.
  - `filename`: (string) The full path to the saved FITS file.
  - `headers`: (object) A key-value map of all the FITS headers.
- `progress`: (object) The progress of the current job.
  - `completed`: (integer) The number of frames captured so far in this job.
  - `total`: (integer) The total number of frames for this job.

#### Example `POST_CAPTURE` Payload:

Based on the provided log, here is an example of the JSON data sent to a post-capture script:

```json
{
  "script_type": "POST_CAPTURE",
  "job": {
    "filter": "Blu",
    "exposure": 1,
    "type": "Light",
    "binning_x": 1,
    "binning_y": 1,
    "target_name": "Kocab"
  },
  "fits": {
    "filename": "/home/stellarmate/Pictures/stella13/Kocab/Light/Blu/Kocab_Light_Blu__11.fits",
    "headers": {
      "AIRMASS": 2.050929,
      "APTDIA": 150,
      "BITPIX": 16,
      "BSCALE": 1,
      "BZERO": 32768,
      "CCD-TEMP": 0,
      "COMMENT": null,
      "DATE-OBS": "2025-06-24T18:37:19.181",
      "DEC": 89.86224,
      "END": null,
      "EQUINOX": 2000,
      "EXPTIME": 1,
      "EXTEND": "T",
      "FILTER": "R",
      "FOCALLEN": 1500,
      "FOCUSPOS": 50000,
      "FOCUSTEM": 0,
      "FRAME": "Light",
      "GAIN": 85,
      "IMAGETYP": "Light Frame",
      "INSTRUME": "CCD Simulator",
      "NAXIS": 2,
      "NAXIS1": 1280,
      "NAXIS2": 1024,
      "OBJCTALT": 29.11354,
      "OBJCTAZ": 359.9999,
      "OBJCTDEC": "89 51 44.05",
      "OBJCTRA": "0 07 31.52",
      "OBJECT": "Kocab",
      "PIERSIDE": "EAST",
      "PIXSIZE1": 5.2,
      "PIXSIZE2": 5.2,
      "RA": 1.881317,
      "ROTATANG": 0,
      "ROWORDER": "TOP-DOWN",
      "SCALE": 0.7151733,
      "SIMPLE": "T",
      "SITELAT": 29.11369,
      "SITELONG": 48.1014,
      "TELESCOP": "Orion EON 120 900@F\\7.5",
      "XBINNING": 1,
      "XPIXSZ": 5.2,
      "YBINNING": 1,
      "YPIXSZ": 5.2
    }
  },
  "progress": {
    "completed": 1,
    "total": 5
  }
}
```

### POST_JOB Payload

The `POST_JOB` payload includes a `progress` object detailing the overall sequence progress.

- `progress`: (object) The progress of the entire sequence queue.
  - `completed`: (integer) The number of jobs completed so far.
  - `total`: (integer) The total number of jobs in the sequence.

#### Example `POST_JOB` Payload:

```json
{
  "script_type": "POST_JOB",
  "job": {
    "filter": "Blu",
    "exposure": 1,
    "type": "Light",
    "binning_x": 1,
    "binning_y": 1,
    "target_name": "Kocab"
  },
  "progress": {
    "completed": 1,
    "total": 3
  }
}
```

## Usage

1.  Make the scripts executable:
    ```bash
    chmod +x post_capture_script.py
    chmod +x post_job_script.py
    ```
2.  In the Ekos Capture module, go to the "Scripts" tab.
3.  For the "Post-capture" or "Post-job" event, enter the full path to the desired script.
4.  Run your capture sequence.
5.  Check the log files (`~/post_capture_log.txt` and `~/post_job_log.txt`) to see the output.
