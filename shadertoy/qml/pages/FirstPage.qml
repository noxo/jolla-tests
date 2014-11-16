/*
  Copyright (C) 2013 Jolla Ltd.
  Contact: Thomas Perl <thomas.perl@jollamobile.com>
  All rights reserved.

  You may use this file under the terms of BSD license as follows:

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Jolla Ltd nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {

    id: page

    onClicked:
    {
        listView.visible = true;
        shaderToy.stop();
    }

    SilicaListView {

        id: listView

        model: ListModel {

            ListElement {
                label: "julia"
                fragmentShader: ":/foo/shaders/julia.f.glsl"
            }

            ListElement {
                label: "boingball"
                fragmentShader: ":/foo/shaders/boingball.f.glsl"
            }

            ListElement {
                label: "grid"
                fragmentShader: ":/foo/shaders/grid.f.glsl"
            }

            ListElement {
                label: "mandel"
                fragmentShader: ":/foo/shaders/mandel.f.glsl"
            }

            ListElement {
                label: "flower"
                fragmentShader: ":/foo/shaders/flower.f.glsl"
            }

            ListElement {
                label: "fly"
                fragmentShader: ":/foo/shaders/fly.f.glsl"
                texture: ":/foo/textures/texl0.jpg"
            }

            ListElement {
                label: "relieftunnel"
                fragmentShader: ":/foo/shaders/relieftunnel.f.glsl"
                texture: ":/foo/textures/texl0.jpg"
            }

            ListElement {
                label: "kaleidoscope"
                fragmentShader: ":/foo/shaders/kaleidoscope.f.glsl"
                texture: ":/foo/textures/texl1.jpg"
            }

            ListElement {
                label: "triangle"
                fragmentShader: ":/foo/shaders/triangle.f.glsl"
                vertexShader: ":/foo/shaders/triangle.v.glsl"
                texture: ":/foo/textures/texl2.jpg"
            }

            ListElement {
                label: "shapes"
                fragmentShader: ":/foo/shaders/shapes.f.glsl"
            }

            ListElement {
                label: "zinvert"
                fragmentShader: ":/foo/shaders/zinvert.f.glsl"
                texture: ":/foo/textures/texl0.jpg"
            }

            ListElement {
                label: "star"
                fragmentShader: ":/foo/shaders/star.f.glsl"
                texture: ":/foo/textures/texl3.jpg"
            }
        }

        anchors.fill: parent

        header: PageHeader {
            title: "Select shader"
        }

        delegate: BackgroundItem {
            id: delegate

            Label {
                x: Theme.paddingLarge
                text: label
                anchors.verticalCenter: parent.verticalCenter
                color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
            onClicked:
            {
                shaderToy.start(fragmentShader, vertexShader, texture);
                // hide listview to get "onClicked" events on page
                listView.visible = false;
            }

        }



        VerticalScrollDecorator {}
    }

}




