/*  Stretch

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QImage>

struct StretchParams1Channel
{
  // Stretch algorithm parameters
  float shadows;;
  float highlights;
  float midtones;
  // The extension parameters are not yet used.
  float shadows_expansion;
  float highlights_expansion;
          
  // The default parameters result in no stretch at all.
  StretchParams1Channel()
  {
    shadows = 0.0;
    highlights = 1.0;
    midtones = 0.5;
    shadows_expansion = 0.0;
    highlights_expansion = 1.0;
  }
};

struct StretchParams
{
  StretchParams1Channel grey_red, green, blue;
};

class Stretch
{
    public:
        /**
         * @brief Stretch Constructor for Stretch class
         * @param image_buffer pointer to the image memory
         * @param width the image width
         * @param height the image height
         * @param channels should be 1 or 3
         * @note The image should either be 1-channel or 3-channel
         * The image buffer is not copied, so it should not be deleted while the object is in use
         */
        explicit Stretch(int width, int height, int channels, int data_type);
        ~Stretch() {}

        /**
         * @brief setParams Sets the stretch parameters. 
         * @param param The desired parameter values.
         * @note This set method used for both 1-channel and 3-channel images.
         * In 1-channel images, the _g and _b parameters are ignored.
         * The parameter scale is 0-1 for all data types.
         */
        void setParams(StretchParams input_params) { params = input_params; }

        /**
         * @brief getParams Returns the stretch parameters (computed by computeParameters()).
         */
        StretchParams getParams() { return params; }

        /**
         * @brief computeParams Automatically generates and sets stretch parameters from the image.
         */
        StretchParams computeParams(const uint8_t *input);

        /**
         * @brief run run the stretch algorithm according to the params given
         * placing the output in output_image.
         * @param input the raw data buffer.
         * @param output_image a QImage pointer that should be the same size as the input if
         * sampling is 1 otherwise, the proper size if the input is downsampled by sampling.
         * @param sampling The sampling parameter. Applies to both width and height.
         * Sampling is applied to the output (that is, with sampling=2, we compute every other output
         * sample both in width and height, so the output would have about 4X fewer pixels.
         */
        void run(uint8_t const *input, QImage *output_image, int sampling=1);

 private:
        // Adjusts input_range for float and double types.
        void recalculateInputRange(const uint8_t *input);

        // Inputs.
        int image_width;
        int image_height;
        int image_channels;
        int input_range;
        int dataType;
  
        // Parameters.
        StretchParams params;
};
