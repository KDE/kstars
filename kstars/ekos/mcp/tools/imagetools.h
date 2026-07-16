/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class Server;
class ToolRegistry;

namespace Tools
{

// Registers image-access tools:
//   image_last_info       — metadata + HFR + star count + dimensions
//   image_last_thumbnail  — base64 JPEG preview (long edge ≤ 1024 px)
//
// Both read from MCP::Server::lastImage() / lastImageFor(), populated by the
// per-camera ISD::Camera::newImage hook installed in Server::hookCamera().
// Every frame producer (Capture queue, PAA, Focus, Align, ad-hoc
// camera_capture, raw INDI control) is covered without per-module wiring.
void initImageTools(ToolRegistry *registry, Server *server);

} // namespace Tools
} // namespace MCP
