FILENAME="traces.tar.gz"

pip install gdown && \
gdown https://drive.google.com/uc?id=1UBM0phzQr-XNM4xxMmVhLdovJRnairkh && \
tar -xvzf "${FILENAME}" && \
rm "${FILENAME}"
