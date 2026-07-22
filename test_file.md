name: Build ProtoPirateTX FAP

on:
  push:
    branches: [ "main" ]
    paths-ignore:
      - '**.md'
      - '**.txt'
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout source
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Checkout Flipper Zero firmware (Momentum)
        uses: actions/checkout@v4
        with:
          repository: 'RocketGod-git/flipperzero-firmware'
          ref: 'dev'
          path: 'flipperzero-firmware'
          fetch-depth: 1

      - name: Copy application source to firmware
        shell: bash
        run: |
          mkdir -p flipperzero-firmware/applications_user/protopirate_tx
          cp -r $(pwd)/* flipperzero-firmware/applications_user/protopirate_tx/ 2>/dev/null || true
          rm -f flipperzero-firmware/applications_user/protopirate_tx/flipperzero-firmware 2>/dev/null || true
          rm -f flipperzero-firmware/applications_user/protopirate_tx/protopirate_app_tx 2>/dev/null || true
          rm -f flipperzero-firmware/applications_user/protopirate_tx/build 2>/dev/null || true
          rm -f flipperzero-firmware/applications_user/protopirate_tx/.github 2>/dev/null || true

      - name: Build application
        working-directory: ./flipperzero-firmware
        shell: bash
        run: |
          pip3 install --quiet pyserial 2>/dev/null || true
          pip3 install --quiet pillow 2>/dev/null || true
          ./fbt fap_protopirate_tx

      - name: Upload FAP artifact
        uses: actions/upload-artifact@v4
        with:
          name: protopirate_tx
          path: flipperzero-firmware/build/**/*.fap
          if-no-files-found: error
