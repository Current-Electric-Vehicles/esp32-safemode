import { useState } from 'react';
import { ButtonSolid, PageHeader, Section } from '../../components/Theme.tsx';
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

  return (
    <>
      <PageHeader>Safemode Update</PageHeader>
      <Section>
      <form>
        <input id="file" type="file" onChange={(e) => onFileChange(e.target.files)}/>
        <ButtonSolid disabled={!file || uploading} onClick={onBeginUpdate} className={'mt save'}>Update</ButtonSolid>
      </form>
      </Section>
    </>
  )
}

export default OTAPage
