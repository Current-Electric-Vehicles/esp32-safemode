import { useState } from 'react';
import { BorderedList, ButtonSolid, Loading, PageHeader, Section } from '../../components/Theme.tsx';
import { API } from '../../components/api.ts';

function OTAPage() {

  const [file, setFile] = useState<File | undefined>(undefined);
  const [uploading, setUploading] = useState<boolean>(false);
  
  const onFileChange = (files: FileList | null) => {
    if (files == null || files.length == 0) {
      setFile(undefined);
      return;
    }
    setFile(files[0]);
  }

  const onBeginUpdate = () => {
    const f = file;
    if (f == undefined) {
      alert("No file selected");
      return;
    }

    setUploading(true);
    API.otaUpdate(f)
      .then((result) => {
        if (result) {
          alert("Update successful, device is now rebootiung");
        } else {
          alert("There was a problem updating the firmware");
        }
      })
      .finally(() => setUploading(false));
  };

  const onBootIntoApp = () => {
    if (!confirm("Are you sure that you want to cancel?")) {
      return;
    }
    API.bootIntoApp()
      .then((result) => {
        if (result) {
          alert("Device is now rebootiung");
        } else {
          alert("There was a problem rebooting into the app");
        }
      });
  };

  return (
    <>
      {uploading && <Loading>Uploading, please wait...</Loading>}
      {!uploading && <>
        <PageHeader>Safemode</PageHeader>
          <Section>
            <BorderedList className={`nopadd`}>
                <li>
                  <p>
                      Use the form below to upload a new firmware image. Please allow adequate 
                      time for the file to upload and the device to update. The device will reboot 
                      after updating.
                  </p>
                  <input className={'inputFile'} id="file" type="file" onChange={(e) => onFileChange(e.target.files)}/>
                </li>
                <li className={'lastItem'}>
                  <ButtonSolid disabled={!file || uploading} onClick={onBeginUpdate} className={'mt save'}>Update</ButtonSolid>
                  <ButtonSolid disabled={uploading} onClick={onBootIntoApp} className={'mt cancel'}>Cancel</ButtonSolid>
                </li>
            </BorderedList>
          </Section>
      </>}
    </>
  )
}

export default OTAPage
