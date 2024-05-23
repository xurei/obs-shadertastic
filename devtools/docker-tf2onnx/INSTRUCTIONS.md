### Building
```bash
docker build . tf2onnx
```
This can take a while to download all dependencies, be patient.

### Usage
```bash
docker run -it --rm -v .:/workdir tf2onnx python3 -m tf2onnx.convert [arguments]
```

### Open BASH
```bash
docker run -it --rm -v .:/workdir tf2onnx bash
```

### Convert tflite to ONNX
```bash
docker run -it --rm -v .:/workdir tf2onnx \
python3 -m tf2onnx.convert --tflite face_detection_full_range.tflite --output face_detection_full_range.onnx
```